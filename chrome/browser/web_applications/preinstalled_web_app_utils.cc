// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_app_utils.h"

#include <memory>
#include <string_view>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "chrome/browser/apps/user_type_filter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/webapps/common/constants.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "ui/events/devices/device_data_manager.h"
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

// kOnlyForNewUsers is an optional boolean. If set and true we will not install
// the app for users that have already run Chrome before.
constexpr char kOnlyForNewUsers[] = "only_for_new_users";

// kOnlyIfPreviouslyPreinstalled is an optional boolean. If set and true we will
// not preinstall the app for new users.
constexpr char kOnlyIfPreviouslyPreinstalled[] =
    "only_if_previously_preinstalled";

// kUserType is an allowlist of user types to install this app for. This must be
// populated otherwise the app won't ever be installed.
// Example: "user_type": ["unmanaged", "managed", "child"]
// See apps::DetermineUserType() for relevant string constants.
constexpr char kUserType[] = "user_type";

// kCreateShortcuts is an optional boolean which controls whether OS
// level shortcuts are created. On Chrome OS this controls whether the app is
// pinned to the shelf.
// The default value of kCreateShortcuts if false.
constexpr char kCreateShortcuts[] = "create_shortcuts";

// kFeatureName is an optional string parameter specifying a feature
// associated with this app. The feature must be present in
// |kPreinstalledAppInstallFeatures| to be applicable.
// If specified:
//  - if the feature is enabled, the app will be installed
//  - if the feature is not enabled, the app will be removed.
constexpr char kFeatureName[] = "feature_name";

// kFeatureNameOrInstalled is an optional string parameter specifying a feature
// associated with this app. The feature must be present in
// |kPreinstalledAppInstallFeatures| to be applicable.
//
// When specified, the app will only be installed when the feature is enabled.
// If the feature is disabled, existing installations will not be removed.
constexpr char kFeatureNameOrInstalled[] = "feature_name_or_installed";

// kDisableIfArcSupported is an optional bool which specifies whether to skip
// install of the app if the device supports Arc (Chrome OS only).
// Defaults to false.
constexpr char kDisableIfArcSupported[] = "disable_if_arc_supported";

// kDisableIfTabletFormFactor is an optional bool which specifies whether to
// skip install of the app if the device is a tablet form factor.
// This is only for Chrome OS tablets, Android does not use any of this code.
// Defaults to false.
constexpr char kDisableIfTabletFormFactor[] = "disable_if_tablet_form_factor";

// kLaunchContainer is a required string which can be "window" or "tab"
// and controls what sort of container the web app is launched in.
constexpr char kLaunchContainer[] = "launch_container";
constexpr char kLaunchContainerTab[] = "tab";
constexpr char kLaunchContainerWindow[] = "window";

// kLaunchQueryParams is an optional string which specifies query parameters to
// add to the start_url when launching the app. If the provided params are a
// substring of start_url's existing params then it will not be added a second
// time.
// Note that substring matches include "param=a" matching in "some_param=abc".
// Extend the implementation in WebAppRegistrar::GetAppLaunchUrl() if this edge
// case needs to be handled differently.
constexpr char kLaunchQueryParams[] = "launch_query_params";

// kLoadAndAwaitServiceWorkerRegistration is an optional bool that specifies
// whether to fetch the |kServiceWorkerRegistrationUrl| after installation to
// allow time for the app to register its service worker. This is done as a
// second pass after install in order to not block the installation of other
// background installed apps. No fetch is made if the service worker has already
// been registered by the |kAppUrl|.
// Defaults to true.
constexpr char kLoadAndAwaitServiceWorkerRegistration[] =
    "load_and_await_service_worker_registration";

// kServiceWorkerRegistrationUrl is an optional string specifying the URL to use
// for the above |kLoadAndAwaitServiceWorkerRegistration|.
// Defaults to the |kAppUrl|.
constexpr char kServiceWorkerRegistrationUrl[] =
    "service_worker_registration_url";

// kUninstallAndReplace is an optional array of strings which specifies App IDs
// which the app is replacing. This will transfer OS attributes (e.g the source
// app's shelf and app list positions on ChromeOS) and then uninstall the source
// app.
constexpr char kUninstallAndReplace[] = "uninstall_and_replace";

// kOnlyUseOfflineManifest is an optional bool.
// If set to true then no network install will be attempted and the app will be
// installed using |kOfflineManifest| data. |kOfflineManifest| must be specified
// in this case.
// If set to false and |kOfflineManifest| is set then it will be used as a
// fallback manifest if the network install fails.
// Defaults to false.
constexpr char kOnlyUseOfflineManifest[] = "only_use_offline_manifest";

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

// "name" manifest value to use for offline install.
constexpr char kOfflineManifestName[] = "name";

// "start_url" manifest value to use for offline install. Can be updated from a
// live manifest if the manifest ID field is specified to match the offline
// start_url.
constexpr char kOfflineManifestStartUrl[] = "start_url";

// "scope" manifest value to use for offline install.
constexpr char kOfflineManifestScope[] = "scope";

// "display" manifest value to use for offline install.
constexpr char kOfflineManifestDisplay[] = "display";

// List of PNG files in the default web app config directory to use as the
// icons for offline install. Will be installed with purpose "any".
constexpr char kOfflineManifestIconAnyPngs[] = "icon_any_pngs";

// List of PNG files in the default web app config directory to use as the
// icons for offline install. Will be installed with purpose "maskable".
constexpr char kOfflineManifestIconMaskablePngs[] = "icon_maskable_pngs";

// Optional 8 value ARGB hex code to use as the "theme_color" manifest value.
// Example:
//   "theme_color_argb_hex": "FFFF0000"
// is equivalent to
//   "theme_color": "red"
constexpr char kOfflineManifestThemeColorArgbHex[] = "theme_color_argb_hex";

// Contains numeric milestone number M like 89 (the Chrome version). The app
// gets updated if browser's binary milestone number goes from <M to >=M.
constexpr char kForceReinstallForMilestone[] = "force_reinstall_for_milestone";

// Contains boolean indicating whether the app installation is requested by
// the device OEM.
constexpr char kOemInstalled[] = "oem_installed";

// Contains boolean indicating weather the app should only be install on devices
// with a built-in touchscreen with stylus support.
constexpr char kDisableIfTouchScreenWithStylusNotSupported[] =
    "disable_if_touchscreen_with_stylus_not_supported";

// Contains boolean that, if set to true, will set the app as the preferred app
// for its supported links after installation. Note that this has no effect if
// the app is already installed as the user may have already updated their
// preference.
constexpr char kIsPreferredAppForSupportedLinks[] =
    "is_preferred_app_for_supported_links";

void EnsureContains(base::Value::List& list, std::string_view value) {
  for (const base::Value& item : list) {
    if (item.is_string() && item.GetString() == value) {
      return;
    }
  }
  list.Append(value);
}

}  // namespace

OptionsOrError ParseConfig(FileUtilsWrapper& file_utils,
                           const base::FilePath& dir,
                           const base::FilePath& file,
                           const base::Value& app_config) {
  if (app_config.type() != base::Value::Type::DICT) {
    return base::StrCat(
        {file.AsUTF8Unsafe(), " was not a dictionary as the top level"});
  }
  const base::Value::Dict& app_config_dict = app_config.GetDict();

  // app_url
  const std::string* app_url_string = app_config_dict.FindString(kAppUrl);
  if (!app_url_string) {
    return base::StrCat({file.AsUTF8Unsafe(), " had a missing ", kAppUrl});
  }
  GURL install_url = GURL(*app_url_string);
  if (!install_url.is_valid()) {
    return base::StrCat({file.AsUTF8Unsafe(), " had an invalid ", kAppUrl, ": ",
                         *app_url_string});
  }

  ExternalInstallOptions options(install_url,
                                 mojom::UserDisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalDefault);
  options.require_manifest = true;
  options.force_reinstall = false;

  // user_type
  const base::Value::List* list = app_config_dict.FindList(kUserType);
  if (!list) {
    return base::StrCat({file.AsUTF8Unsafe(), " missing ", kUserType});
  }
  for (const auto& item : *list) {
    if (!item.is_string()) {
      return base::StrCat({file.AsUTF8Unsafe(), " has invalid ", kUserType,
                           item.DebugString()});
    }
    options.user_type_allowlist.push_back(item.GetString());
  }
  if (options.user_type_allowlist.empty()) {
    return base::StrCat({file.AsUTF8Unsafe(), " has empty ", kUserType});
  }

  // feature_name
  const std::string* feature_name = app_config_dict.FindString(kFeatureName);
  if (feature_name) {
    options.gate_on_feature = *feature_name;
  }

  // feature_name_or_installed
  const std::string* feature_name_or_installed =
      app_config_dict.FindString(kFeatureNameOrInstalled);
  if (feature_name_or_installed) {
    options.gate_on_feature_or_installed = *feature_name_or_installed;
  }
  // only_for_new_users
  const base::Value* value = app_config_dict.Find(kOnlyForNewUsers);
  if (value) {
    if (!value->is_bool()) {
      return base::StrCat(
          {file.AsUTF8Unsafe(), " had an invalid ", kOnlyForNewUsers});
    }
    options.only_for_new_users = value->GetBool();
  }

  // only_if_previously_preinstalled
  value = app_config_dict.Find(kOnlyIfPreviouslyPreinstalled);
  if (value) {
    if (!value->is_bool()) {
      return base::StrCat({file.AsUTF8Unsafe(), " had an invalid ",
                           kOnlyIfPreviouslyPreinstalled});
    }
    options.only_if_previously_preinstalled = value->GetBool();
  }

  // hide_from_user
  bool hide_from_user = false;
  value = app_config_dict.Find(kHideFromUser);
  if (value) {
    if (!value->is_bool()) {
      return base::StrCat(
          {file.AsUTF8Unsafe(), " had an invalid ", kHideFromUser});
    }
    hide_from_user = value->GetBool();
  }
  options.add_to_applications_menu = !hide_from_user;
  options.add_to_search = !hide_from_user;
  options.add_to_management = !hide_from_user;

  // create_shortcuts
  bool create_shortcuts = false;
  value = app_config_dict.Find(kCreateShortcuts);
  if (value) {
    if (!value->is_bool()) {
      return base::StrCat(
          {file.AsUTF8Unsafe(), " had an invalid ", kCreateShortcuts});
    }
    create_shortcuts = value->GetBool();
  }
  options.add_to_desktop = create_shortcuts;
  options.add_to_quick_launch_bar = create_shortcuts;

  // It doesn't make sense to hide the app and also create shortcuts for it.
  DCHECK(!(hide_from_user && create_shortcuts));

  // disable_if_arc_supported
  value = app_config_dict.Find(kDisableIfArcSupported);
  if (value) {
    if (!value->is_bool()) {
      return base::StrCat(
          {file.AsUTF8Unsafe(), " had an invalid ", kDisableIfArcSupported});
    }
    options.disable_if_arc_supported = value->GetBool();
  }

  // disable_if_tablet_form_factor
  value = app_config_dict.Find(kDisableIfTabletFormFactor);
  if (value) {
    if (!value->is_bool()) {
      return base::StrCat({file.AsUTF8Unsafe(), " had an invalid ",
                           kDisableIfTabletFormFactor});
    }
    options.disable_if_tablet_form_factor = value->GetBool();
  }

  // launch_container
  const std::string* launch_container_str =
      app_config_dict.FindString(kLaunchContainer);
  if (!launch_container_str) {
    return base::StrCat(
        {file.AsUTF8Unsafe(), " had an invalid ", kLaunchContainer});
  }
  if (*launch_container_str == kLaunchContainerTab) {
    options.user_display_mode = mojom::UserDisplayMode::kBrowser;
  } else if (*launch_container_str == kLaunchContainerWindow) {
    options.user_display_mode = mojom::UserDisplayMode::kStandalone;
  } else {
    return base::StrCat({file.AsUTF8Unsafe(), " had an invalid ",
                         kLaunchContainer, ": ", *launch_container_str});
  }

  // launch_query_params
  value = app_config_dict.Find(kLaunchQueryParams);
  if (value) {
    if (!value->is_string()) {
      return base::StrCat(
          {file.AsUTF8Unsafe(), " had an invalid ", kLaunchQueryParams});
    }
    options.launch_query_params = value->GetString();
  }

  // load_and_await_service_worker_registration
  value = app_config_dict.Find(kLoadAndAwaitServiceWorkerRegistration);
  if (value) {
    if (!value->is_bool()) {
      return base::StrCat({file.AsUTF8Unsafe(), " had an invalid ",
                           kLoadAndAwaitServiceWorkerRegistration});
    }
    options.load_and_await_service_worker_registration = value->GetBool();
  }

  // service_worker_registration_url
  value = app_config_dict.Find(kServiceWorkerRegistrationUrl);
  if (value) {
    if (!options.load_and_await_service_worker_registration) {
      return base::StrCat({file.AsUTF8Unsafe(), " should not specify a ",
                           kServiceWorkerRegistrationUrl, " while ",
                           kLoadAndAwaitServiceWorkerRegistration,
                           " is disabled"});
    }
    if (!value->is_string()) {
      return base::StrCat({file.AsUTF8Unsafe(), " had an invalid ",
                           kServiceWorkerRegistrationUrl});
    }
    options.service_worker_registration_url.emplace(value->GetString());
    if (!options.service_worker_registration_url->is_valid()) {
      return base::StrCat({file.AsUTF8Unsafe(), " had an invalid ",
                           kServiceWorkerRegistrationUrl});
    }
  }

  // uninstall_and_replace
  value = app_config_dict.Find(kUninstallAndReplace);
  if (value) {
    if (!value->is_list()) {
      return base::StrCat(
          {file.AsUTF8Unsafe(), " had an invalid ", kUninstallAndReplace});
    }
    const base::Value::List& uninstall_and_replace_values = value->GetList();

    for (const auto& app_id_value : uninstall_and_replace_values) {
      if (!app_id_value.is_string()) {
        return base::StrCat({file.AsUTF8Unsafe(), " had an invalid ",
                             kUninstallAndReplace, " entry"});
      }
      options.uninstall_and_replace.push_back(app_id_value.GetString());
    }
  }

  // only_use_offline_manifest
  value = app_config_dict.Find(kOnlyUseOfflineManifest);
  if (value) {
    if (!value->is_bool()) {
      return base::StrCat(
          {file.AsUTF8Unsafe(), " had an invalid ", kOnlyUseOfflineManifest});
    }
    options.only_use_app_info_factory = value->GetBool();
  }

  // offline_manifest
  value = app_config_dict.Find(kOfflineManifest);
  if (value && value->is_dict()) {
    WebAppInstallInfoFactoryOrError offline_manifest_result =
        ParseOfflineManifest(file_utils, dir, file, *value);
    if (std::string* error =
            absl::get_if<std::string>(&offline_manifest_result)) {
      return std::move(*error);
    }
    options.app_info_factory =
        std::move(absl::get<WebAppInstallInfoFactory>(offline_manifest_result));
  }

  if (options.only_use_app_info_factory && !options.app_info_factory) {
    return base::StrCat({file.AsUTF8Unsafe(), kOnlyUseOfflineManifest,
                         " set with no ", kOfflineManifest, " available"});
  }

  // force_reinstall_for_milestone
  value = app_config_dict.Find(kForceReinstallForMilestone);
  if (value) {
    if (!value->is_int()) {
      return base::StrCat({file.AsUTF8Unsafe(), " had an invalid ",
                           kForceReinstallForMilestone});
    }
    options.force_reinstall_for_milestone = value->GetInt();
  }

  // oem_installed
  value = app_config_dict.Find(kOemInstalled);
  if (value) {
    if (!value->is_bool()) {
      return base::StrCat(
          {file.AsUTF8Unsafe(), " had an invalid ", kOemInstalled});
    }
    options.oem_installed = value->GetBool();
  }

  // disable_if_touchscreen_with_stylus_not_supported
  value = app_config_dict.Find(kDisableIfTouchScreenWithStylusNotSupported);
  if (value) {
    if (!value->is_bool()) {
      return base::StrCat({file.AsUTF8Unsafe(), " had an invalid ",
                           kDisableIfTouchScreenWithStylusNotSupported});
    }
    options.disable_if_touchscreen_with_stylus_not_supported = value->GetBool();
  }

  // is_preferred_app_for_supported_links
  value = app_config_dict.Find(kIsPreferredAppForSupportedLinks);
  if (value) {
    if (!value->is_bool()) {
      return base::StrCat({file.AsUTF8Unsafe(), " had an invalid ",
                           kIsPreferredAppForSupportedLinks});
    }
    options.is_preferred_app_for_supported_links = value->GetBool();
  }

  return options;
}

IconBitmapsOrError ParseOfflineManifestIconBitmaps(
    FileUtilsWrapper& file_utils,
    const base::FilePath& dir,
    const base::FilePath& manifest_file,
    const char* icon_key,
    const base::Value::List& icon_files) {
  std::map<SquareSizePx, SkBitmap> icon_bitmaps;

  for (const base::Value& icon_file : icon_files) {
    if (!icon_file.is_string()) {
      return base::unexpected(base::StrCat(
          {manifest_file.AsUTF8Unsafe(), " ", kOfflineManifest, " ", icon_key,
           " ", icon_file.DebugString(), " invalid."}));
    }
    base::FilePath icon_path = dir.AppendASCII(icon_file.GetString());
    std::string icon_data;
    if (!file_utils.ReadFileToString(icon_path, &icon_data)) {
      return base::unexpected(base::StrCat(
          {manifest_file.AsUTF8Unsafe(), " ", kOfflineManifest, " ", icon_key,
           " ", icon_file.DebugString(), " failed to read."}));
    }
    SkBitmap bitmap;
    if (!gfx::PNGCodec::Decode(
            reinterpret_cast<const unsigned char*>(icon_data.c_str()),
            icon_data.size(), &bitmap)) {
      return base::unexpected(base::StrCat(
          {manifest_file.AsUTF8Unsafe(), " ", kOfflineManifest, " ", icon_key,
           " ", icon_file.DebugString(), " failed to decode."}));
    }
    if (bitmap.width() != bitmap.height()) {
      return base::unexpected(base::StrCat(
          {manifest_file.AsUTF8Unsafe(), " ", kOfflineManifest, " ", icon_key,
           " ", icon_file.DebugString(),
           " must be square: ", base::NumberToString(bitmap.width()), "x",
           base::NumberToString(bitmap.height())}));
    }
    icon_bitmaps[bitmap.width()] = std::move(bitmap);
  }

  return icon_bitmaps;
}

WebAppInstallInfoFactoryOrError ParseOfflineManifest(
    FileUtilsWrapper& file_utils,
    const base::FilePath& dir,
    const base::FilePath& file,
    const base::Value& offline_manifest) {
  const base::Value::Dict& offline_manifest_dict = offline_manifest.GetDict();

  // start_url
  const std::string* start_url_string =
      offline_manifest_dict.FindString(kOfflineManifestStartUrl);
  if (!start_url_string) {
    return base::StrCat({file.AsUTF8Unsafe(), " ", kOfflineManifest, " ",
                         kOfflineManifestStartUrl, " missing or invalid."});
  }
  GURL start_url = GURL(*start_url_string);
  if (!start_url.is_valid()) {
    return base::StrCat({file.AsUTF8Unsafe(), " ", kOfflineManifest, " ",
                         kOfflineManifestStartUrl,
                         " invalid: ", *start_url_string});
  }

  // Offline manifest doesn't have a way to specify a manifest ID so all offline
  // manifests use start_url to derive their identity. If any offline manifest
  // needs to change start_url then a separate manifest_id will need to be
  // specified and loaded here.
  webapps::ManifestId manifest_id =
      GenerateManifestIdFromStartUrlOnly(start_url);
  auto app_info = std::make_unique<WebAppInstallInfo>(manifest_id, start_url);

  // name
  const std::string* name_string =
      offline_manifest_dict.FindString(kOfflineManifestName);
  if (!name_string) {
    return base::StrCat({file.AsUTF8Unsafe(), " ", kOfflineManifest, " ",
                         kOfflineManifestName, " missing or invalid."});
  }
  if (!base::UTF8ToUTF16(name_string->data(), name_string->size(),
                         &app_info->title) ||
      app_info->title.empty()) {
    return base::StrCat({file.AsUTF8Unsafe(), " ", kOfflineManifest, " ",
                         kOfflineManifestName, " invalid: ", *name_string});
  }

  // scope
  const std::string* scope_string =
      offline_manifest_dict.FindString(kOfflineManifestScope);
  if (!scope_string) {
    return base::StrCat({file.AsUTF8Unsafe(), " ", kOfflineManifest, " ",
                         kOfflineManifestScope, " missing or invalid."});
  }
  app_info->scope = GURL(*scope_string);
  if (!app_info->scope.is_valid()) {
    return base::StrCat({file.AsUTF8Unsafe(), " ", kOfflineManifest, " ",
                         kOfflineManifestScope, " invalid: ", *scope_string});
  }
  if (!base::StartsWith(app_info->start_url().path(), app_info->scope.path(),
                        base::CompareCase::SENSITIVE)) {
    return base::StrCat(
        {file.AsUTF8Unsafe(), " ", kOfflineManifest, " ", kOfflineManifestScope,
         " (", app_info->start_url().spec(), ") not within ",
         kOfflineManifestScope, " (", app_info->scope.spec(), ")."});
  }

  // display
  const std::string* display_string =
      offline_manifest_dict.FindString(kOfflineManifestDisplay);
  if (!display_string) {
    return base::StrCat({file.AsUTF8Unsafe(), " ", kOfflineManifest, " ",
                         kOfflineManifestDisplay, " missing or invalid."});
  }
  DisplayMode display = blink::DisplayModeFromString(*display_string);
  if (display == DisplayMode::kUndefined) {
    return base::StrCat({file.AsUTF8Unsafe(), " ", kOfflineManifest, " ",
                         kOfflineManifestDisplay,
                         " invalid: ", *display_string});
  }
  app_info->display_mode = display;

  // icon_any_pngs || icon_maskable_pngs
  const base::Value::List* icon_any_files =
      offline_manifest_dict.FindList(kOfflineManifestIconAnyPngs);
  const base::Value::List* icon_maskable_files =
      offline_manifest_dict.FindList(kOfflineManifestIconMaskablePngs);

  if (!icon_any_files && !icon_maskable_files) {
    return base::StrCat({file.AsUTF8Unsafe(), " ", kOfflineManifest, " ",
                         kOfflineManifestIconAnyPngs, " and ",
                         kOfflineManifestIconMaskablePngs,
                         " missing or invalid."});
  }

  if (icon_any_files) {
    if (icon_any_files->empty()) {
      return base::StrCat({file.AsUTF8Unsafe(), " ", kOfflineManifest, " ",
                           kOfflineManifestIconAnyPngs, " empty."});
    }

    ASSIGN_OR_RETURN(app_info->icon_bitmaps.any,
                     ParseOfflineManifestIconBitmaps(
                         file_utils, dir, file, kOfflineManifestIconAnyPngs,
                         *icon_any_files));
  }

  if (icon_maskable_files) {
    if (icon_maskable_files->empty()) {
      return base::StrCat({file.AsUTF8Unsafe(), " ", kOfflineManifest, " ",
                           kOfflineManifestIconMaskablePngs, " empty."});
    }

    ASSIGN_OR_RETURN(
        app_info->icon_bitmaps.maskable,
        ParseOfflineManifestIconBitmaps(file_utils, dir, file,
                                        kOfflineManifestIconMaskablePngs,
                                        *icon_maskable_files));
  }

  // theme_color_argb_hex (optional)
  const base::Value* theme_color_value =
      offline_manifest_dict.Find(kOfflineManifestThemeColorArgbHex);
  if (theme_color_value) {
    const std::string* theme_color_argb_hex =
        theme_color_value->is_string() ? &theme_color_value->GetString()
                                       : nullptr;
    SkColor theme_color;
    if (!theme_color_argb_hex ||
        !base::HexStringToUInt(*theme_color_argb_hex, &theme_color)) {
      return base::StrCat({file.AsUTF8Unsafe(), " ", kOfflineManifest, " ",
                           kOfflineManifestThemeColorArgbHex,
                           " invalid: ", theme_color_value->DebugString()});
    }
    app_info->theme_color = SkColorSetA(theme_color, SK_AlphaOPAQUE);
  }

  return base::BindRepeating(
      [](const std::unique_ptr<WebAppInstallInfo>& original) {
        return std::make_unique<WebAppInstallInfo>(original->Clone());
      },
      std::move(app_info));
}

bool IsReinstallPastMilestoneNeeded(
    std::string_view last_preinstall_synchronize_milestone_str,
    std::string_view current_milestone_str,
    int force_reinstall_for_milestone) {
  int last_preinstall_synchronize_milestone = 0;
  if (!base::StringToInt(last_preinstall_synchronize_milestone_str,
                         &last_preinstall_synchronize_milestone)) {
    return false;
  }

  int current_milestone = 0;
  if (!base::StringToInt(current_milestone_str, &current_milestone)) {
    return false;
  }

  return last_preinstall_synchronize_milestone <
             force_reinstall_for_milestone &&
         current_milestone >= force_reinstall_for_milestone;
}

bool WasAppMigratedToWebApp(Profile* profile, const std::string& app_id) {
  const base::Value::List& migrated_apps =
      profile->GetPrefs()->GetList(webapps::kWebAppsMigratedPreinstalledApps);

  for (const auto& val : migrated_apps) {
    if (val.is_string() && val.GetString() == app_id) {
      return true;
    }
  }

  return false;
}

void MarkAppAsMigratedToWebApp(Profile* profile,
                               const std::string& app_id,
                               bool was_migrated) {
  ScopedListPrefUpdate update(profile->GetPrefs(),
                              webapps::kWebAppsMigratedPreinstalledApps);
  base::Value::List& update_list = update.Get();
  if (was_migrated) {
    EnsureContains(update_list, app_id);
  } else {
    update_list.EraseValue(base::Value(app_id));
  }
}

bool WasMigrationRun(Profile* profile, std::string_view feature_name) {
  const base::Value::List& migrated_features =
      profile->GetPrefs()->GetList(prefs::kWebAppsDidMigrateDefaultChromeApps);

  for (const auto& val : migrated_features) {
    if (val.is_string() && val.GetString() == feature_name) {
      return true;
    }
  }

  return false;
}

void SetMigrationRun(Profile* profile,
                     std::string_view feature_name,
                     bool was_migrated) {
  ScopedListPrefUpdate update(profile->GetPrefs(),
                              prefs::kWebAppsDidMigrateDefaultChromeApps);
  base::Value::List& update_list = update.Get();
  if (was_migrated) {
    EnsureContains(update_list, feature_name);
  } else {
    update_list.EraseValue(base::Value(feature_name));
  }
}

bool WasPreinstalledAppUninstalled(Profile* profile,
                                   const std::string& app_id) {
  const base::Value::List& uninstalled_apps =
      profile->GetPrefs()->GetList(prefs::kWebAppsUninstalledDefaultChromeApps);

  for (const auto& val : uninstalled_apps) {
    if (val.is_string() && val.GetString() == app_id) {
      return true;
    }
  }

  return false;
}

void MarkPreinstalledAppAsUninstalled(Profile* profile,
                                      const std::string& app_id) {
  if (WasPreinstalledAppUninstalled(profile, app_id)) {
    return;
  }
  ScopedListPrefUpdate update(profile->GetPrefs(),
                              prefs::kWebAppsUninstalledDefaultChromeApps);
  EnsureContains(update.Get(), app_id);
}

std::optional<bool> DeviceHasStylusEnabledTouchscreen() {
  if (!ui::DeviceDataManager::HasInstance() ||
      !ui::DeviceDataManager::GetInstance()->AreDeviceListsComplete()) {
    return std::nullopt;
  }

  for (const ui::TouchscreenDevice& device :
       ui::DeviceDataManager::GetInstance()->GetTouchscreenDevices()) {
    if (device.has_stylus &&
        device.type == ui::InputDeviceType::INPUT_DEVICE_INTERNAL) {
      return true;
    }
  }
  return false;
}

}  // namespace web_app
