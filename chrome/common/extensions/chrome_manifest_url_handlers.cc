// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/chrome_manifest_url_handlers.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/url_constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/manifest_url_handlers.h"
#include "extensions/common/permissions/api_permission.h"

#if defined(OS_CHROMEOS)
#include "ash/keyboard/ui/resources/keyboard_resource_util.h"
#endif

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

namespace {

const char kOverrideExtentUrlPatternFormat[] = "chrome://%s/*";

}  // namespace

namespace chrome_manifest_urls {
const GURL& GetDevToolsPage(const Extension* extension) {
  return ManifestURL::Get(extension, keys::kDevToolsPage);
}
}

URLOverrides::URLOverrides() {
}

URLOverrides::~URLOverrides() {
}

static base::LazyInstance<URLOverrides::URLOverrideMap>::DestructorAtExit
    g_empty_url_overrides = LAZY_INSTANCE_INITIALIZER;

// static
const URLOverrides::URLOverrideMap& URLOverrides::GetChromeURLOverrides(
    const Extension* extension) {
  URLOverrides* url_overrides = static_cast<URLOverrides*>(
      extension->GetManifestData(keys::kChromeURLOverrides));
  return url_overrides ? url_overrides->chrome_url_overrides_
                       : g_empty_url_overrides.Get();
}

DevToolsPageHandler::DevToolsPageHandler() {
}

DevToolsPageHandler::~DevToolsPageHandler() {
}

bool DevToolsPageHandler::Parse(Extension* extension, base::string16* error) {
  std::unique_ptr<ManifestURL> manifest_url(new ManifestURL);
  std::string devtools_str;
  if (!extension->manifest()->GetString(keys::kDevToolsPage, &devtools_str)) {
    *error = base::ASCIIToUTF16(errors::kInvalidDevToolsPage);
    return false;
  }
  manifest_url->url_ = extension->GetResourceURL(devtools_str);
  extension->SetManifestData(keys::kDevToolsPage, std::move(manifest_url));
  PermissionsParser::AddAPIPermission(extension, APIPermission::kDevtools);
  return true;
}

base::span<const char* const> DevToolsPageHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kDevToolsPage};
  return kKeys;
}

URLOverridesHandler::URLOverridesHandler() {
}

URLOverridesHandler::~URLOverridesHandler() {
}

bool URLOverridesHandler::Parse(Extension* extension, base::string16* error) {
  const base::DictionaryValue* overrides = NULL;
  if (!extension->manifest()->GetDictionary(keys::kChromeURLOverrides,
                                            &overrides)) {
    *error = base::ASCIIToUTF16(errors::kInvalidChromeURLOverrides);
    return false;
  }
  std::unique_ptr<URLOverrides> url_overrides(new URLOverrides);
  // Validate that the overrides are all strings
  for (base::DictionaryValue::Iterator iter(*overrides); !iter.IsAtEnd();
       iter.Advance()) {
    const std::string& page = iter.key();
    std::string val;
    // Restrict override pages to a list of supported URLs.
    bool is_allowed_host = page == chrome::kChromeUINewTabHost ||
                           page == chrome::kChromeUIBookmarksHost ||
                           page == chrome::kChromeUIHistoryHost;
#if defined(OS_CHROMEOS)
    is_allowed_host = is_allowed_host ||
                      page == chrome::kChromeUIActivationMessageHost ||
                      page == keyboard::kKeyboardHost;
#endif

    if (!is_allowed_host || !iter.value().GetAsString(&val)) {
      *error = base::ASCIIToUTF16(errors::kInvalidChromeURLOverrides);
      return false;
    }
    // Replace the entry with a fully qualified chrome-extension:// URL.
    url_overrides->chrome_url_overrides_[page] = extension->GetResourceURL(val);

    // For component extensions, add override URL to extent patterns.
    if (extension->is_legacy_packaged_app() &&
        extension->location() == Manifest::COMPONENT) {
      URLPattern pattern(URLPattern::SCHEME_CHROMEUI);
      std::string url =
          base::StringPrintf(kOverrideExtentUrlPatternFormat, page.c_str());
      if (pattern.Parse(url) != URLPattern::ParseResult::kSuccess) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidURLPatternError, url);
        return false;
      }
      extension->AddWebExtentPattern(pattern);
    }
  }

  // An extension may override at most one page.
  if (overrides->size() > 1) {
    *error = base::ASCIIToUTF16(errors::kMultipleOverrides);
    return false;
  }

  // If this is an NTP override extension, add the NTP override permission.
  if (url_overrides->chrome_url_overrides_.count(chrome::kChromeUINewTabHost)) {
    PermissionsParser::AddAPIPermission(extension,
                                        APIPermission::kNewTabPageOverride);
  }

  extension->SetManifestData(keys::kChromeURLOverrides,
                             std::move(url_overrides));

  return true;
}

bool URLOverridesHandler::Validate(
    const Extension* extension,
    std::string* error,
    std::vector<InstallWarning>* warnings) const {
  const URLOverrides::URLOverrideMap& overrides =
      URLOverrides::GetChromeURLOverrides(extension);
  if (overrides.empty())
    return true;

  for (const auto& entry : overrides) {
    base::FilePath relative_path =
        file_util::ExtensionURLToRelativeFilePath(entry.second);
    base::FilePath resource_path =
        extension->GetResource(relative_path).GetFilePath();
    if (resource_path.empty() || !base::PathExists(resource_path)) {
      *error = ErrorUtils::FormatErrorMessage(errors::kFileNotFound,
                                              relative_path.AsUTF8Unsafe());
      return false;
    }
  }
  return true;
}

base::span<const char* const> URLOverridesHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kChromeURLOverrides};
  return kKeys;
}

}  // namespace extensions
