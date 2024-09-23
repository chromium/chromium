// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"

#include <memory>

#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "components/app_constants/constants.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace keys = manifest_keys;
namespace values = manifest_values;
namespace errors = manifest_errors;

namespace {

bool ReadLaunchDimension(const extensions::Manifest* manifest,
                         const char* key,
                         int* target,
                         bool is_valid_container,
                         std::u16string* error) {
  if (const base::Value* temp = manifest->FindPath(key)) {
    if (!is_valid_container) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidLaunchValueContainer,
          key);
      return false;
    }
    if (!temp->is_int() || temp->GetInt() < 0) {
      *target = 0;
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidLaunchValue,
          key);
      return false;
    }
    *target = temp->GetInt();
  }
  return true;
}

static base::LazyInstance<AppLaunchInfo>::DestructorAtExit
    g_empty_app_launch_info = LAZY_INSTANCE_INITIALIZER;

const AppLaunchInfo& GetAppLaunchInfo(const Extension* extension) {
  AppLaunchInfo* info = static_cast<AppLaunchInfo*>(
      extension->GetManifestData(keys::kLaunch));
  return info ? *info : g_empty_app_launch_info.Get();
}

}  // namespace

AppLaunchInfo::AppLaunchInfo() = default;

AppLaunchInfo::~AppLaunchInfo() = default;

// static
const std::string& AppLaunchInfo::GetLaunchLocalPath(
    const Extension* extension) {
  return GetAppLaunchInfo(extension).launch_local_path_;
}

// static
const GURL& AppLaunchInfo::GetLaunchWebURL(const Extension* extension) {
  return GetAppLaunchInfo(extension).launch_web_url_;
}

// static
apps::LaunchContainer AppLaunchInfo::GetLaunchContainer(
    const Extension* extension) {
  return GetAppLaunchInfo(extension).launch_container_;
}

// static
int AppLaunchInfo::GetLaunchWidth(const Extension* extension) {
  return GetAppLaunchInfo(extension).launch_width_;
}

// static
int AppLaunchInfo::GetLaunchHeight(const Extension* extension) {
  return GetAppLaunchInfo(extension).launch_height_;
}

// static
GURL AppLaunchInfo::GetFullLaunchURL(const Extension* extension) {
  const AppLaunchInfo& info = GetAppLaunchInfo(extension);
  if (info.launch_local_path_.empty()) {
    return info.launch_web_url_;
  } else {
    return extension->url().Resolve(info.launch_local_path_);
  }
}

bool AppLaunchInfo::Parse(Extension* extension, std::u16string* error) {
  if (!LoadLaunchURL(extension, error) ||
      !LoadLaunchContainer(extension, error))
    return false;
  return true;
}

bool AppLaunchInfo::LoadLaunchURL(Extension* extension, std::u16string* error) {
  // Launch URL can be either local (to chrome-extension:// root) or an absolute
  // web URL.
  if (const base::Value* temp =
          extension->manifest()->FindPath(keys::kLaunchLocalPath);
      temp) {
    if (extension->manifest()->FindPath(keys::kLaunchWebURL)) {
      *error = errors::kLaunchPathAndURLAreExclusive;
      return false;
    }

    if (extension->manifest()->FindPath(keys::kWebURLs)) {
      *error = errors::kLaunchPathAndExtentAreExclusive;
      return false;
    }

    if (!temp->is_string()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidLaunchValue,
          keys::kLaunchLocalPath);
      return false;
    }
    const std::string launch_path = temp->GetString();

    // Ensure the launch path is a valid relative URL.
    GURL resolved = extension->url().Resolve(launch_path);
    if (!resolved.is_valid() ||
        resolved.DeprecatedGetOriginAsURL() != extension->url()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidLaunchValue,
          keys::kLaunchLocalPath);
      return false;
    }

    launch_local_path_ = launch_path;
  } else if (temp = extension->manifest()->FindPath(keys::kLaunchWebURL);
             temp) {
    if (!temp->is_string()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidLaunchValue,
          keys::kLaunchWebURL);
      return false;
    }

    auto set_launch_web_url_error = [&]() {
      *error = ErrorUtils::FormatErrorMessageUTF16(errors::kInvalidLaunchValue,
                                                   keys::kLaunchWebURL);
    };
    // Ensure the launch web URL is a valid absolute URL and web extent scheme.
    GURL url(temp->GetString());
    if (!url.is_valid()) {
      set_launch_web_url_error();
      return false;
    }

    URLPattern pattern(Extension::kValidWebExtentSchemes);
    if (!pattern.IsValidScheme(url.scheme())) {
      set_launch_web_url_error();
      return false;
    }

    launch_web_url_ = url;
  } else if (extension->is_legacy_packaged_app()) {
    *error = errors::kLaunchURLRequired;
    return false;
  }

  // For the Chrome component app, override launch url to new tab.
  if (extension->id() == app_constants::kChromeAppId) {
    launch_web_url_ = GURL(chrome::kChromeUINewTabURL);
    return true;
  }

  // If there is no extent, we default the extent based on the launch URL.
  if (extension->web_extent().is_empty() && !launch_web_url_.is_empty()) {
    URLPattern pattern(Extension::kValidWebExtentSchemes);
    if (!pattern.SetScheme("*")) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidLaunchValue,
          keys::kLaunchWebURL);
      return false;
    }
    pattern.SetHost(launch_web_url_.host());
    pattern.SetPath("/*");
    extension->AddWebExtentPattern(pattern);
  }

  return true;
}

bool AppLaunchInfo::LoadLaunchContainer(Extension* extension,
                                        std::u16string* error) {
  const base::Value* tmp_launcher_container =
      extension->manifest()->FindPath(keys::kLaunchContainer);
  if (tmp_launcher_container == nullptr)
    return true;

  if (!tmp_launcher_container->is_string()) {
    *error = errors::kInvalidLaunchContainer;
    return false;
  }
  const std::string launch_container_string =
      tmp_launcher_container->GetString();

  if (launch_container_string == values::kLaunchContainerPanelDeprecated) {
    launch_container_ = apps::LaunchContainer::kLaunchContainerPanelDeprecated;
  } else if (launch_container_string == values::kLaunchContainerTab) {
    launch_container_ = apps::LaunchContainer::kLaunchContainerTab;
  } else {
    *error = errors::kInvalidLaunchContainer;
    return false;
  }

  // TODO(manucornet): Remove this special behavior now that panels are
  // deprecated.
  bool can_specify_initial_size =
      launch_container_ ==
      apps::LaunchContainer::kLaunchContainerPanelDeprecated;

  // Validate the container width if present.
  if (!ReadLaunchDimension(extension->manifest(),
                           keys::kLaunchWidth,
                           &launch_width_,
                           can_specify_initial_size,
                           error)) {
    return false;
  }

  // Validate container height if present.
  if (!ReadLaunchDimension(extension->manifest(),
                           keys::kLaunchHeight,
                           &launch_height_,
                           can_specify_initial_size,
                           error)) {
    return false;
  }

  return true;
}

void AppLaunchInfo::OverrideLaunchURL(Extension* extension,
                                      GURL override_url) {
  if (!override_url.is_valid()) {
    DLOG(WARNING) << "Invalid override url given for " << extension->name();
    return;
  }
  if (override_url.has_port()) {
    DLOG(WARNING) << "Override URL passed for " << extension->name()
                  << " should not contain a port.  Removing it.";

    GURL::Replacements remove_port;
    remove_port.ClearPort();
    override_url = override_url.ReplaceComponents(remove_port);
  }

  launch_web_url_ = override_url;

  URLPattern pattern(Extension::kValidWebExtentSchemes);
  URLPattern::ParseResult result = pattern.Parse(override_url.spec());
  DCHECK_EQ(result, URLPattern::ParseResult::kSuccess);
  pattern.SetPath(pattern.path() + '*');
  extension->AddWebExtentPattern(pattern);
}

AppLaunchManifestHandler::AppLaunchManifestHandler() = default;

AppLaunchManifestHandler::~AppLaunchManifestHandler() = default;

bool AppLaunchManifestHandler::Parse(Extension* extension,
                                     std::u16string* error) {
  std::unique_ptr<AppLaunchInfo> info(new AppLaunchInfo);
  if (!info->Parse(extension, error))
    return false;
  extension->SetManifestData(keys::kLaunch, std::move(info));
  return true;
}

bool AppLaunchManifestHandler::AlwaysParseForType(Manifest::Type type) const {
  return type == Manifest::TYPE_LEGACY_PACKAGED_APP;
}

base::span<const char* const> AppLaunchManifestHandler::Keys() const {
  static constexpr const char* kKeys[] = {
      keys::kLaunchLocalPath, keys::kLaunchWebURL, keys::kLaunchContainer,
      keys::kLaunchHeight, keys::kLaunchWidth};
  return kKeys;
}

}  // namespace extensions
