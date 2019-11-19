// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"

#include <memory>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "components/cloud_devices/common/cloud_devices_urls.h"
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
                         base::string16* error) {
  const base::Value* temp = NULL;
  if (manifest->Get(key, &temp)) {
    if (!is_valid_container) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidLaunchValueContainer,
          key);
      return false;
    }
    if (!temp->GetAsInteger(target) || *target < 0) {
      *target = 0;
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidLaunchValue,
          key);
      return false;
    }
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

AppLaunchInfo::AppLaunchInfo()
    : launch_container_(LaunchContainer::kLaunchContainerTab),
      launch_width_(0),
      launch_height_(0) {}

AppLaunchInfo::~AppLaunchInfo() {
}

// static
const std::string& AppLaunchInfo::GetLaunchLocalPath(
    const Extension* extension) {
  return GetAppLaunchInfo(extension).launch_local_path_;
}

// static
const GURL& AppLaunchInfo::GetLaunchWebURL(
    const Extension* extension) {
  return GetAppLaunchInfo(extension).launch_web_url_;
}

// static
extensions::LaunchContainer AppLaunchInfo::GetLaunchContainer(
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
  if (info.launch_local_path_.empty())
    return info.launch_web_url_;
  else
    return extension->url().Resolve(info.launch_local_path_);
}

bool AppLaunchInfo::Parse(Extension* extension, base::string16* error) {
  if (!LoadLaunchURL(extension, error) ||
      !LoadLaunchContainer(extension, error))
    return false;
  return true;
}

bool AppLaunchInfo::LoadLaunchURL(Extension* extension, base::string16* error) {
  const base::Value* temp = NULL;

  // Launch URL can be either local (to chrome-extension:// root) or an absolute
  // web URL.
  if (extension->manifest()->Get(keys::kLaunchLocalPath, &temp)) {
    if (extension->manifest()->Get(keys::kLaunchWebURL, NULL)) {
      *error = base::ASCIIToUTF16(errors::kLaunchPathAndURLAreExclusive);
      return false;
    }

    if (extension->manifest()->Get(keys::kWebURLs, NULL)) {
      *error = base::ASCIIToUTF16(errors::kLaunchPathAndExtentAreExclusive);
      return false;
    }

    std::string launch_path;
    if (!temp->GetAsString(&launch_path)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidLaunchValue,
          keys::kLaunchLocalPath);
      return false;
    }

    // Ensure the launch path is a valid relative URL.
    GURL resolved = extension->url().Resolve(launch_path);
    if (!resolved.is_valid() || resolved.GetOrigin() != extension->url()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidLaunchValue,
          keys::kLaunchLocalPath);
      return false;
    }

    launch_local_path_ = launch_path;
  } else if (extension->manifest()->Get(keys::kLaunchWebURL, &temp)) {
    std::string launch_url;
    if (!temp->GetAsString(&launch_url)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidLaunchValue,
          keys::kLaunchWebURL);
      return false;
    }

    // Ensure the launch web URL is a valid absolute URL and web extent scheme.
    GURL url(launch_url);
    URLPattern pattern(Extension::kValidWebExtentSchemes);
    if (extension->from_bookmark()) {
      // System Web Apps are bookmark apps that point to chrome:// URLs.
      int valid_schemes = Extension::kValidBookmarkAppSchemes;
      if (extension->location() == Manifest::EXTERNAL_COMPONENT)
        valid_schemes |= URLPattern::SCHEME_CHROMEUI;
      pattern.SetValidSchemes(valid_schemes);
    }
    if ((!url.is_valid() || !pattern.SetScheme(url.scheme()))) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidLaunchValue,
          keys::kLaunchWebURL);
      return false;
    }

    launch_web_url_ = url;
  } else if (extension->is_legacy_packaged_app()) {
    *error = base::ASCIIToUTF16(errors::kLaunchURLRequired);
    return false;
  }

  // For the Chrome component app, override launch url to new tab.
  if (extension->id() == extension_misc::kChromeAppId) {
    launch_web_url_ = GURL(chrome::kChromeUINewTabURL);
    return true;
  }

  // If there is no extent, we default the extent based on the launch URL.
  // Skip this step if the extension is from a bookmark app, as they are
  // permissionless.
  if (extension->web_extent().is_empty() && !launch_web_url_.is_empty() &&
      !extension->from_bookmark()) {
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

  // In order for the --apps-gallery-url switch to work with the gallery
  // process isolation, we must insert any provided value into the component
  // app's launch url and web extent.
  if (extension->id() == extensions::kWebStoreAppId) {
    std::string gallery_url_str =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kAppsGalleryURL);

    // Empty string means option was not used.
    if (!gallery_url_str.empty()) {
      GURL gallery_url(gallery_url_str);
      OverrideLaunchURL(extension, gallery_url);
    }
  } else if (extension->id() == extension_misc::kCloudPrintAppId) {
    // In order for the --type=service switch to work, we must update the launch
    // URL and web extent.
    GURL url =
        cloud_devices::GetCloudPrintRelativeURL("enable_chrome_connector");
    if (!url.is_empty()) {
      OverrideLaunchURL(extension, url);
    }
  }

  return true;
}

bool AppLaunchInfo::LoadLaunchContainer(Extension* extension,
                                        base::string16* error) {
  const base::Value* tmp_launcher_container = NULL;
  if (!extension->manifest()->Get(keys::kLaunchContainer,
                                  &tmp_launcher_container))
    return true;

  std::string launch_container_string;
  if (!tmp_launcher_container->GetAsString(&launch_container_string)) {
    *error = base::ASCIIToUTF16(errors::kInvalidLaunchContainer);
    return false;
  }

  if (launch_container_string == values::kLaunchContainerPanelDeprecated) {
    launch_container_ = LaunchContainer::kLaunchContainerPanelDeprecated;
  } else if (launch_container_string == values::kLaunchContainerTab) {
    launch_container_ = LaunchContainer::kLaunchContainerTab;
  } else {
    *error = base::ASCIIToUTF16(errors::kInvalidLaunchContainer);
    return false;
  }

  // TODO(manucornet): Remove this special behavior now that panels are
  // deprecated.
  bool can_specify_initial_size =
      launch_container_ == LaunchContainer::kLaunchContainerPanelDeprecated;

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

AppLaunchManifestHandler::AppLaunchManifestHandler() {
}

AppLaunchManifestHandler::~AppLaunchManifestHandler() {
}

bool AppLaunchManifestHandler::Parse(Extension* extension,
                                     base::string16* error) {
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
