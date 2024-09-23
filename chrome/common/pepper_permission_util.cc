// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/pepper_permission_util.h"

#include <vector>

#include "base/command_line.h"
#include "base/hash/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"

using extensions::Extension;
using extensions::Manifest;
using extensions::SharedModuleInfo;

namespace {

std::string HashHost(const std::string& host) {
  return base::HexEncode(base::SHA1Hash(base::as_byte_span(host)));
}

bool HostIsInSet(const std::string& host, const std::set<std::string>& set) {
  return set.count(host) > 0 || set.count(HashHost(host)) > 0;
}

}  // namespace

bool IsExtensionOrSharedModuleAllowed(
    const GURL& url,
    const extensions::ExtensionSet* extension_set,
    const std::set<std::string>& allowlist) {
  if (!url.is_valid() || !url.SchemeIs(extensions::kExtensionScheme))
    return false;

  const std::string host = url.host();
  if (HostIsInSet(host, allowlist))
    return true;

  // Check the modules that are imported by this extension to see if any of them
  // is allowed.
  const Extension* extension =
      extension_set ? extension_set->GetByID(host) : nullptr;
  if (!extension)
    return false;

  typedef std::vector<SharedModuleInfo::ImportInfo> ImportInfoVector;
  const ImportInfoVector& imports = SharedModuleInfo::GetImports(extension);
  for (auto it = imports.begin(); it != imports.end(); ++it) {
    const Extension* imported_extension =
        extension_set->GetByID(it->extension_id);
    if (imported_extension &&
        SharedModuleInfo::IsSharedModule(imported_extension) &&
        HostIsInSet(it->extension_id, allowlist)) {
      return true;
    }
  }

  return false;
}

bool IsHostAllowedByCommandLine(const GURL& url,
                                const extensions::ExtensionSet* extension_set,
                                const char* command_line_switch) {
  if (!url.is_valid())
    return false;

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  const std::string allowed_list =
      command_line.GetSwitchValueASCII(command_line_switch);
  if (allowed_list.empty())
    return false;

  const std::string host = url.host();
  if (allowed_list == "*") {
    // For now, we only allow packaged and platform apps in this wildcard.
    if (!extension_set || !url.SchemeIs(extensions::kExtensionScheme))
      return false;

    const Extension* extension = extension_set->GetByID(host);
    return extension &&
        (extension->GetType() == Manifest::TYPE_LEGACY_PACKAGED_APP ||
         extension->GetType() == Manifest::TYPE_PLATFORM_APP);
  }

  base::StringTokenizer t(allowed_list, ",");
  while (t.GetNext()) {
    if (t.token_piece() == host)
      return true;
  }

  return false;
}
