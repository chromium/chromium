// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/external_web_app_utils.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/apps/user_type_filter.h"
#include "chrome/browser/web_applications/components/external_app_install_features.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "ui/gfx/codec/png_codec.h"

namespace web_app {

namespace {

// kAppUrl is a required string specifying a URL inside the scope of the web
// app that contains a link to the app manifest.
constexpr char kAppUrl[] = "app_url";

// kHideFromUser is an optional boolean which controls whether we add
// a shortcut to the relevant OS surface i.e. Application folder on macOS, Start
// Menu on Windows and Linux, and launcher on Chrome OS. Defaults to false if
// missing. If true, we also don't show the app in search or in app management
// on Chrome OS.
constexpr char kHideFromUser[] = "hide_from_user";

// kCreateShortcuts is an optional boolean which controls whether OS
// level shortcuts are created. On Chrome OS this controls whether the app is
// pinned to the shelf.
// The default value of kCreateShortcuts if false.
constexpr char kCreateShortcuts[] = "create_shortcuts";

// kFeatureName is an optional string parameter specifying a feature
// associated with this app. The feature must be present in
// |kExternalAppInstallFeatures| to be applicable.
// If specified:
//  - if the feature is enabled, the app will be installed
//  - if the feature is not enabled, the app will be removed.
constexpr char kFeatureName[] = "feature_name";

// kLaunchContainer is a required string which can be "window" or "tab"
// and controls what sort of container the web app is launched in.
constexpr char kLaunchContainer[] = "launch_container";
constexpr char kLaunchContainerTab[] = "tab";
constexpr char kLaunchContainerWindow[] = "window";

// kUninstallAndReplace is an optional array of strings which specifies App IDs
// which the app is replacing. This will transfer OS attributes (e.g the source
// app's shelf and app list positions on ChromeOS) and then uninstall the source
// app.
constexpr char kUninstallAndReplace[] = "uninstall_and_replace";

// kOfflineManifest is a dictionary of manifest field values to use as an
// install to avoid the expense of fetching the install URL to download the
// app's true manifest. Next time the user visits the app it will undergo a
// manifest update check and correct any differences from the site (except for
// name and start_url).
//
// Why not use blink::ManifestParser?
// blink::ManifestParser depends on substantial sections of the CSS parser which
// is infeasible to run outside of the renderer process.
constexpr char kOfflineManifest[] = "offline_manifest";

// "name" manifest value to use for offline install. Cannot be updated.
// TODO(crbug.com/1119699): Allow updating of name.
constexpr char kOfflineManifestName[] = "name";

// "start_url" manifest value to use for offline install. Cannot be updated.
// TODO(crbug.com/1119699): Allow updating of start_url.
constexpr char kOfflineManifestStartUrl[] = "start_url";

// "scope" manifest value to use for offline install.
constexpr char kOfflineManifestScope[] = "scope";

// "display" manifest value to use for offline install.
constexpr char kOfflineManifestDisplay[] = "display";

// List of PNG files in the default web app config directory to use as the
// icons for offline install. Will be installed with purpose "any".
constexpr char kOfflineManifestIconAnyPngs[] = "icon_any_pngs";

// Optional 8 value ARGB hex code to use as the "theme_color" manifest value.
// Example:
//   "theme_color_argb_hex": "FFFF0000"
// is equivalent to
//   "theme_color": "red"
constexpr char kOfflineManifestThemeColorArgbHex[] = "theme_color_argb_hex";

}  // namespace

base::Optional<ExternalInstallOptions> ParseConfig(
    FileUtilsWrapper& file_utils,
    const base::FilePath& dir,
    const base::FilePath& file,
    const std::string& user_type,
    const base::Value& app_config) {
  if (app_config.type() != base::Value::Type::DICTIONARY) {
    LOG(ERROR) << file << " was not a dictionary as the top level";
    return base::nullopt;
  }

  if (!apps::UserTypeMatchesJsonUserType(
          user_type, /*app_id=*/file.MaybeAsASCII(), &app_config,
          /*default_user_types=*/nullptr)) {
    // Already logged.
    return base::nullopt;
  }

  const base::Value* value =
      app_config.FindKeyOfType(kFeatureName, base::Value::Type::STRING);
  if (value) {
    // TODO(crbug.com/1104696): Add metrics for whether the app was
    // enabled/disabled by the feature.
    const std::string& feature_name = value->GetString();
    VLOG(1) << file << " checking feature " << feature_name;
    if (!IsExternalAppInstallFeatureEnabled(feature_name)) {
      VLOG(1) << file << " feature not enabled";
      return base::nullopt;
    }
  }

  value = app_config.FindKeyOfType(kAppUrl, base::Value::Type::STRING);
  if (!value) {
    LOG(ERROR) << file << " had a missing " << kAppUrl;
    return base::nullopt;
  }
  GURL app_url(value->GetString());
  if (!app_url.is_valid()) {
    LOG(ERROR) << file << " had an invalid " << kAppUrl;
    return base::nullopt;
  }

  bool hide_from_user = false;
  value = app_config.FindKey(kHideFromUser);
  if (value) {
    if (!value->is_bool()) {
      LOG(ERROR) << file << " had an invalid " << kHideFromUser;
      return base::nullopt;
    }
    hide_from_user = value->GetBool();
  }

  bool create_shortcuts = false;
  value = app_config.FindKey(kCreateShortcuts);
  if (value) {
    if (!value->is_bool()) {
      LOG(ERROR) << file << " had an invalid " << kCreateShortcuts;
      return base::nullopt;
    }
    create_shortcuts = value->GetBool();
  }

  // It doesn't make sense to hide the app and also create shortcuts for it.
  DCHECK(!(hide_from_user && create_shortcuts));

  value = app_config.FindKeyOfType(kLaunchContainer, base::Value::Type::STRING);
  if (!value) {
    LOG(ERROR) << file << " had an invalid " << kLaunchContainer;
    return base::nullopt;
  }
  std::string launch_container_str = value->GetString();
  auto user_display_mode = DisplayMode::kBrowser;
  if (launch_container_str == kLaunchContainerTab) {
    user_display_mode = DisplayMode::kBrowser;
  } else if (launch_container_str == kLaunchContainerWindow) {
    user_display_mode = DisplayMode::kStandalone;
  } else {
    LOG(ERROR) << file << " had an invalid " << kLaunchContainer;
    return base::nullopt;
  }

  value = app_config.FindKey(kUninstallAndReplace);
  std::vector<AppId> uninstall_and_replace_ids;
  if (value) {
    if (!value->is_list()) {
      LOG(ERROR) << file << " had an invalid " << kUninstallAndReplace;
      return base::nullopt;
    }
    base::Value::ConstListView uninstall_and_replace_values = value->GetList();

    bool had_error = false;
    for (const auto& app_id_value : uninstall_and_replace_values) {
      if (!app_id_value.is_string()) {
        had_error = true;
        LOG(ERROR) << file << " had an invalid " << kUninstallAndReplace
                   << " entry";
        break;
      }
      uninstall_and_replace_ids.push_back(app_id_value.GetString());
    }
    if (had_error)
      return base::nullopt;
  }

  value = app_config.FindDictKey(kOfflineManifest);
  WebApplicationInfoFactory app_info_factory;
  if (value)
    app_info_factory = ParseOfflineManifest(file_utils, dir, file, *value);

  ExternalInstallOptions install_options(
      std::move(app_url), user_display_mode,
      ExternalInstallSource::kExternalDefault);
  install_options.add_to_applications_menu = !hide_from_user;
  install_options.add_to_search = !hide_from_user;
  install_options.add_to_management = !hide_from_user;
  install_options.add_to_desktop = create_shortcuts;
  install_options.add_to_quick_launch_bar = create_shortcuts;
  install_options.require_manifest = true;
  install_options.uninstall_and_replace = std::move(uninstall_and_replace_ids);
  install_options.app_info_factory = std::move(app_info_factory);

  return install_options;
}

WebApplicationInfoFactory ParseOfflineManifest(
    FileUtilsWrapper& file_utils,
    const base::FilePath& dir,
    const base::FilePath& file,
    const base::Value& offline_manifest) {
  WebApplicationInfo app_info;

  // name
  const std::string* name_string =
      offline_manifest.FindStringKey(kOfflineManifestName);
  if (!name_string) {
    LOG(ERROR) << file << " " << kOfflineManifest << " " << kOfflineManifestName
               << " missing or invalid.";
    return {};
  }
  if (!base::UTF8ToUTF16(name_string->data(), name_string->size(),
                         &app_info.title) ||
      app_info.title.empty()) {
    LOG(ERROR) << file << " " << kOfflineManifest << " " << kOfflineManifestName
               << " invalid: " << *name_string;
    return {};
  }

  // start_url
  const std::string* start_url_string =
      offline_manifest.FindStringKey(kOfflineManifestStartUrl);
  if (!start_url_string) {
    LOG(ERROR) << file << " " << kOfflineManifest << " "
               << kOfflineManifestStartUrl << " missing or invalid.";
    return {};
  }
  app_info.app_url = GURL(*start_url_string);
  if (!app_info.app_url.is_valid()) {
    LOG(ERROR) << file << " " << kOfflineManifest << " "
               << kOfflineManifestStartUrl << " invalid: " << *start_url_string;
    return {};
  }

  // scope
  const std::string* scope_string =
      offline_manifest.FindStringKey(kOfflineManifestScope);
  if (!scope_string) {
    LOG(ERROR) << file << " " << kOfflineManifest << " "
               << kOfflineManifestScope << " missing or invalid.";
    return {};
  }
  app_info.scope = GURL(*scope_string);
  if (!app_info.scope.is_valid()) {
    LOG(ERROR) << file << " " << kOfflineManifest << " "
               << kOfflineManifestScope << " invalid: " << *scope_string;
    return {};
  }
  if (!base::StartsWith(app_info.app_url.path(), app_info.scope.path(),
                        base::CompareCase::SENSITIVE)) {
    LOG(ERROR) << file << " " << kOfflineManifest << " "
               << kOfflineManifestScope << " (" << app_info.app_url
               << ") not within " << kOfflineManifestScope << " ("
               << app_info.scope << ").";
    return {};
  }

  // display
  const std::string* display_string =
      offline_manifest.FindStringKey(kOfflineManifestDisplay);
  if (!display_string) {
    LOG(ERROR) << file << " " << kOfflineManifest << " "
               << kOfflineManifestDisplay << " missing or invalid.";
    return {};
  }
  DisplayMode display = blink::DisplayModeFromString(*display_string);
  if (display == DisplayMode::kUndefined) {
    LOG(ERROR) << file << " " << kOfflineManifest << " "
               << kOfflineManifestDisplay << " invalid: " << display_string;
    return {};
  }
  app_info.display_mode = display;

  // icon_any_pngs
  const base::Value* icon_files =
      offline_manifest.FindListKey(kOfflineManifestIconAnyPngs);
  if (!icon_files || icon_files->GetList().empty()) {
    LOG(ERROR) << file << " " << kOfflineManifest << " "
               << kOfflineManifestIconAnyPngs << " missing, empty or invalid.";
    return {};
  }
  for (const base::Value& icon_file : icon_files->GetList()) {
    if (!icon_file.is_string()) {
      LOG(ERROR) << file << " " << kOfflineManifest << " "
                 << kOfflineManifestIconAnyPngs << " " << icon_file
                 << " invalid.";
      return {};
    }

    base::FilePath icon_path = dir.AppendASCII(icon_file.GetString());
    std::string icon_data;
    if (!file_utils.ReadFileToString(icon_path, &icon_data)) {
      LOG(ERROR) << file << " " << kOfflineManifest << " "
                 << kOfflineManifestIconAnyPngs << " " << icon_file
                 << " failed to read.";
      return {};
    }

    SkBitmap bitmap;
    if (!gfx::PNGCodec::Decode(
            reinterpret_cast<const unsigned char*>(icon_data.c_str()),
            icon_data.size(), &bitmap)) {
      LOG(ERROR) << file << " " << kOfflineManifest << " "
                 << kOfflineManifestIconAnyPngs << " " << icon_file
                 << " failed to decode.";
      return {};
    }

    if (bitmap.width() != bitmap.height()) {
      LOG(ERROR) << file << " " << kOfflineManifest << " "
                 << kOfflineManifestIconAnyPngs << " " << icon_file
                 << " must be square: " << bitmap.width() << "x"
                 << bitmap.height();
      return {};
    }

    app_info.icon_bitmaps_any[bitmap.width()] = std::move(bitmap);
  }
  DCHECK(!app_info.icon_bitmaps_any.empty());

  // theme_color_argb_hex (optional)
  const base::Value* theme_color_value =
      offline_manifest.FindKey(kOfflineManifestThemeColorArgbHex);
  if (theme_color_value) {
    const std::string* theme_color_argb_hex =
        theme_color_value->is_string() ? &theme_color_value->GetString()
                                       : nullptr;
    SkColor theme_color;
    if (!theme_color_argb_hex ||
        !base::HexStringToUInt(*theme_color_argb_hex, &theme_color)) {
      LOG(ERROR) << file << " " << kOfflineManifest << " "
                 << kOfflineManifestThemeColorArgbHex
                 << " invalid: " << *theme_color_value;
      return {};
    }
    app_info.theme_color = SkColorSetA(theme_color, SK_AlphaOPAQUE);
  }

  return base::BindRepeating(
      &std::make_unique<WebApplicationInfo, const WebApplicationInfo&>,
      std::move(app_info));
}

}  // namespace web_app
