// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/app/crashpad.h"

#include <vector>

#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "build/branding_buildflags.h"
#include "components/crash/core/app/crash_reporter_client.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"

namespace crash_reporter {

namespace {

const std::map<std::string, std::string>& GetProcessSimpleAnnotations() {
  static std::map<std::string, std::string> annotations = []() -> auto {
    std::map<std::string, std::string> process_annotations;
    @autoreleasepool {
      NSBundle* outer_bundle = base::mac::OuterBundle();
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      process_annotations["prod"] = "Chrome_iOS";
#else
      NSString* product = base::mac::ObjCCast<NSString>([outer_bundle
          objectForInfoDictionaryKey:base::mac::CFToNSCast(kCFBundleNameKey)]);
      process_annotations["prod"] =
          base::SysNSStringToUTF8(product).append("_iOS");
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      // Empty means stable.
      const bool allow_empty_channel = true;
#else
      const bool allow_empty_channel = false;
#endif
      NSString* channel = base::mac::ObjCCast<NSString>(
          [outer_bundle objectForInfoDictionaryKey:@"KSChannelID"]);
      // Must be a developer build.
      if (!allow_empty_channel && (!channel || !channel.length))
        channel = @"developer";
      process_annotations["channel"] = base::SysNSStringToUTF8(channel);
      NSString* version =
          base::mac::ObjCCast<NSString>([base::mac::FrameworkBundle()
              objectForInfoDictionaryKey:@"CFBundleShortVersionString"]);
      process_annotations["ver"] = base::SysNSStringToUTF8(version);
      process_annotations["plat"] = std::string("iOS");
      process_annotations["crashpad"] = std::string("yes");
    }  // @autoreleasepool
    return process_annotations;
  }
  ();
  return annotations;
}

}  // namespace

void ProcessIntermediateDumps(
    const std::map<std::string, std::string>& annotations) {
  GetCrashpadClient().ProcessIntermediateDumps(annotations);
}

namespace internal {

base::FilePath PlatformCrashpadInitialization(
    bool initial_client,
    bool browser_process,
    bool embedded_handler,
    const std::string& user_data_dir,
    const base::FilePath& exe_path,
    const std::vector<std::string>& initial_arguments) {
  base::FilePath database_path;  // Only valid in the browser process.
  DCHECK(!embedded_handler);     // This is not used on iOS.
  DCHECK(exe_path.empty());      // This is not used on iOS.
  DCHECK(initial_arguments.empty());

  if (initial_client) {
    @autoreleasepool {
      CrashReporterClient* crash_reporter_client = GetCrashReporterClient();
      crash_reporter_client->GetCrashDumpLocation(&database_path);
      std::string url = crash_reporter_client->GetUploadUrl();
      GetCrashpadClient().StartCrashpadInProcessHandler(
          database_path, url, GetProcessSimpleAnnotations());
    }  // @autoreleasepool
  }

  return database_path;
}

}  // namespace internal
}  // namespace crash_reporter
