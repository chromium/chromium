// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copies files from argv[1] to argv[2]

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <unistd.h>

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/code_signature.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/apps/app_shim/code_signature_mac.h"

namespace {

base::apple::ScopedCFTypeRef<CFStringRef>
BuildParentAppRequirementFromFrameworkRequirementString(
    CFStringRef framwork_requirement) {
  // Make sure the framework bundle requirement is in the expected format.
  // It should start with 'identifier "' and have at least 2 quotes. This allows
  // us to easily find the end of the "identifier" portion of the requirement so
  // we can remove it.
  CFIndex len = CFStringGetLength(framwork_requirement);
  base::apple::ScopedCFTypeRef<CFArrayRef> quote_ranges(
      CFStringCreateArrayWithFindResults(nullptr, framwork_requirement,
                                         CFSTR("\""), CFRangeMake(0, len), 0));
  if (!CFStringHasPrefix(framwork_requirement, CFSTR("identifier \"")) ||
      !quote_ranges || CFArrayGetCount(quote_ranges.get()) < 2) {
    LOG(ERROR) << "Framework bundle requirement is malformed.";
    return base::apple::ScopedCFTypeRef<CFStringRef>(nullptr);
  }

  // Get the index of the second quote.
  CFIndex second_quote_index =
      static_cast<const CFRange*>(CFArrayGetValueAtIndex(quote_ranges.get(), 1))
          ->location;

  // Make sure there is something to read after the second quote.
  if (second_quote_index + 1 >= len) {
    LOG(ERROR) << "Framework bundle requirement is too short";
    return base::apple::ScopedCFTypeRef<CFStringRef>(nullptr);
  }

  // Build the app shim requirement. Keep the data from the framework bundle
  // requirement starting after second quote.
  base::apple::ScopedCFTypeRef<CFStringRef> parent_app_requirement_string(
      CFStringCreateWithSubstring(
          nullptr, framwork_requirement,
          CFRangeMake(second_quote_index + 5, len - second_quote_index - 5)));
  return parent_app_requirement_string;
}

// Creates a requirement for the parent app based on the framework bundle's
// designated requirement.
//
// Returns a non-null requirement or the reason why the requirement could not
// be created.
base::expected<base::apple::ScopedCFTypeRef<SecRequirementRef>,
               apps::MissingRequirementReason>
CreateParentAppRequirement() {
  ASSIGN_OR_RETURN(auto framework_requirement_string,
                   apps::FrameworkBundleDesignatedRequirementString());

  base::apple::ScopedCFTypeRef<CFStringRef> parent_requirement_string =
      BuildParentAppRequirementFromFrameworkRequirementString(
          framework_requirement_string.get());
  if (!parent_requirement_string) {
    return base::unexpected(apps::MissingRequirementReason::Error);
  }

  return apps::RequirementFromString(parent_requirement_string.get());
}

// Ensure that the parent process is Chromium.
// This prevents this tool from being used to bypass binary authorization tools
// such as Santa.
//
// Returns whether the parent process's code signature is trusted:
// - True if the framework bundle is unsigned (there's nothing to verify).
// - True if the parent process satisfies the constructed designated requirement
// tailored for the parent app based on the framework bundle's requirement.
// - False otherwise.
bool ValidateParentProcess(std::string_view info_plist_xml) {
  base::expected<base::apple::ScopedCFTypeRef<SecRequirementRef>,
                 apps::MissingRequirementReason>
      parent_app_requirement = CreateParentAppRequirement();
  if (!parent_app_requirement.has_value()) {
    switch (parent_app_requirement.error()) {
      case apps::MissingRequirementReason::NoOrAdHocSignature:
        // Parent validation is not required because framework bundle is not
        // code-signed or is ad-hoc code-signed.
        return true;
      case apps::MissingRequirementReason::Error:
        // Framework bundle is code-signed however we were unable to create the
        // parent app requirement. Deny.
        // CreateParentAppRequirement already did the
        // base::debug::DumpWithoutCrashing, possibly on a previous call. We can
        // return false here without any additional explanation.
        return false;
    }
  }

  // Perform dynamic validation only as Chrome.app's dynamic signature may not
  // match its on-disk signature if there is an update pending.
  OSStatus status = base::mac::ProcessIdIsSignedAndFulfillsRequirement_DoNotUse(
      getppid(), parent_app_requirement.value().get(),
      base::mac::SignatureValidationType::DynamicOnly, info_plist_xml);
  return status == errSecSuccess;
}

}  // namespace

extern "C" {
// The entry point into the shortcut copier process. This is not
// a user API.
__attribute__((visibility("default"))) int ChromeWebAppShortcutCopierMain(
    int argc,
    char** argv);
}

// Copies files from argv[1] to argv[2]
//
// When using ad-hoc signing for web app shims, the final app shim must be
// written to disk by this helper tool. This separate helper tool exists so that
// binary authorization tools, such as Santa, can transitively trust app shims
// that it creates without trusting all files written by Chrome. This allows app
// shims to be trusted by the binary authorization tool despite having only
// ad-hoc code signatures.
//
// argv[3] is the Info.plist contents of Chrome. This is needed to validate the
// dynamic code signature of the running application as the Info.plist file on
// disk may have changed if there is an update pending. The passed-in data is
// validated against a hash recorded in the code signature before being used
// during requirement validation.
int ChromeWebAppShortcutCopierMain(int argc, char** argv) {
  if (argc != 4) {
    return 1;
  }

  // Override the path to the framework value so that it has a sensible value.
  // This tool lives within the Helpers subdirectory of the framework, so the
  // versioned path is two levels upwards.
  base::FilePath executable_path =
      base::PathService::CheckedGet(base::FILE_EXE);
  base::apple::SetOverrideFrameworkBundlePath(
      executable_path.DirName().DirName());

  if (!ValidateParentProcess(argv[3])) {
    return 1;
  }

  base::FilePath staging_path = base::FilePath::FromUTF8Unsafe(argv[1]);
  base::FilePath dst_app_path = base::FilePath::FromUTF8Unsafe(argv[2]);

  if (!base::CopyDirectory(staging_path, dst_app_path, true)) {
    LOG(ERROR) << "Copying app from " << staging_path << " to " << dst_app_path
               << " failed.";
    return 2;
  }

  return 0;
}
