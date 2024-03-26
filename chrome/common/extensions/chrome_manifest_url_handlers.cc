// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/chrome_manifest_url_handlers.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/extensions/api/chrome_url_overrides.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/manifest_url_handlers.h"
#include "extensions/common/permissions/api_permission.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

namespace {

const char kOverrideExtentUrlPatternFormat[] = "chrome://%s/*";
using ChromeUrlOverridesKeys = api::chrome_url_overrides::ManifestKeys;

}  // namespace

namespace chrome_manifest_urls {
const GURL& GetDevToolsPage(const Extension* extension) {
  return ManifestURL::Get(extension, keys::kDevToolsPage);
}
}

URLOverrides::URLOverrides() = default;
URLOverrides::~URLOverrides() = default;

// static
const URLOverrides::URLOverrideMap& URLOverrides::GetChromeURLOverrides(
    const Extension* extension) {
  static const base::NoDestructor<URLOverrides::URLOverrideMap>
      empty_url_overrides;
  URLOverrides* url_overrides = static_cast<URLOverrides*>(
      extension->GetManifestData(ChromeUrlOverridesKeys::kChromeUrlOverrides));
  return url_overrides ? url_overrides->chrome_url_overrides_
                       : *empty_url_overrides;
}

DevToolsPageHandler::DevToolsPageHandler() = default;
DevToolsPageHandler::~DevToolsPageHandler() = default;

bool DevToolsPageHandler::Parse(Extension* extension, std::u16string* error) {
  std::unique_ptr<ManifestURL> manifest_url(new ManifestURL);
  const std::string* devtools_str =
      extension->manifest()->FindStringPath(keys::kDevToolsPage);
  if (!devtools_str) {
    *error = errors::kInvalidDevToolsPage;
    return false;
  }
  GURL url = extension->GetResourceURL(*devtools_str);
  // SharedModuleInfo::IsImportedPath() does not require knowledge of data from
  // extension, so we can call it right here in Parse() and not Validate() and
  // do not need to specify DevToolsPageHandler::PrerequisiteKeys()
  const bool is_extension_url = url.SchemeIs(kExtensionScheme) &&
                                url.host_piece() == extension->id() &&
                                !SharedModuleInfo::IsImportedPath(url.path());
  if (!is_extension_url) {
    *error = errors::kInvalidDevToolsPage;
    return false;
  }
  manifest_url->url_ = std::move(url);
  extension->SetManifestData(keys::kDevToolsPage, std::move(manifest_url));
  PermissionsParser::AddAPIPermission(extension,
                                      mojom::APIPermissionID::kDevtools);
  return true;
}

base::span<const char* const> DevToolsPageHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kDevToolsPage};
  return kKeys;
}

bool DevToolsPageHandler::Validate(
    const Extension* extension,
    std::string* error,
    std::vector<InstallWarning>* warnings) const {
  const GURL& url = chrome_manifest_urls::GetDevToolsPage(extension);
  const base::FilePath relative_path =
      file_util::ExtensionURLToRelativeFilePath(url);
  const base::FilePath resource_path =
      extension->GetResource(relative_path).GetFilePath();
  if (resource_path.empty() || !base::PathExists(resource_path)) {
    const std::string message = ErrorUtils::FormatErrorMessage(
        errors::kFileNotFound, relative_path.AsUTF8Unsafe());
    warnings->emplace_back(message, keys::kDevToolsPage);
  }
  return true;
}

URLOverridesHandler::URLOverridesHandler() = default;
URLOverridesHandler::~URLOverridesHandler() = default;

bool URLOverridesHandler::Parse(Extension* extension, std::u16string* error) {
  ChromeUrlOverridesKeys manifest_keys;
  if (!ChromeUrlOverridesKeys::ParseFromDictionary(
          extension->manifest()->available_values(), manifest_keys, *error)) {
    return false;
  }

  using UrlOverrideInfo = api::chrome_url_overrides::UrlOverrideInfo;
  auto url_overrides = std::make_unique<URLOverrides>();
  auto property_map =
      std::map<const char*,
               std::reference_wrapper<const std::optional<std::string>>>{
          {UrlOverrideInfo::kNewtab,
           std::ref(manifest_keys.chrome_url_overrides.newtab)},
          {UrlOverrideInfo::kBookmarks,
           std::ref(manifest_keys.chrome_url_overrides.bookmarks)},
          {UrlOverrideInfo::kHistory,
           std::ref(manifest_keys.chrome_url_overrides.history)},
          {UrlOverrideInfo::kActivationmessage,
           std::ref(manifest_keys.chrome_url_overrides.activationmessage)},
          {UrlOverrideInfo::kKeyboard,
           std::ref(manifest_keys.chrome_url_overrides.keyboard)}};

  for (const auto& property : property_map) {
    if (!property.second.get())
      continue;

    // Replace the entry with a fully qualified chrome-extension:// URL.
    url_overrides->chrome_url_overrides_[property.first] =
        extension->GetResourceURL(*property.second.get());

    // For component extensions, add override URL to extent patterns.
    if (extension->is_legacy_packaged_app() &&
        extension->location() == mojom::ManifestLocation::kComponent) {
      URLPattern pattern(URLPattern::SCHEME_CHROMEUI);
      std::string url =
          base::StringPrintf(kOverrideExtentUrlPatternFormat, property.first);
      if (pattern.Parse(url) != URLPattern::ParseResult::kSuccess) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidURLPatternError, url);
        return false;
      }
      extension->AddWebExtentPattern(pattern);
    }
  }

  // An extension may override at most one page.
  if (url_overrides->chrome_url_overrides_.size() > 1u) {
    *error = errors::kMultipleOverrides;
    return false;
  }

  // If this is an NTP override extension, add the NTP override permission.
  if (manifest_keys.chrome_url_overrides.newtab) {
    PermissionsParser::AddAPIPermission(
        extension, mojom::APIPermissionID::kNewTabPageOverride);
  }

  extension->SetManifestData(ChromeUrlOverridesKeys::kChromeUrlOverrides,
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
  static constexpr const char* kKeys[] = {
      ChromeUrlOverridesKeys::kChromeUrlOverrides};
  return kKeys;
}

}  // namespace extensions
