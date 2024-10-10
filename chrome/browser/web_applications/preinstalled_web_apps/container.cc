// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/container.h"

#include <memory>
#include <vector>

#include "ash/constants/web_app_id_constants.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "chrome/browser/apps/user_type_filter.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_app_definition_utils.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/grit/preinstalled_web_apps_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/webapps/common/web_app_id.h"
#include "url/gurl.h"

namespace web_app {
namespace {

// Returns the appropriate activation time threshold to use `for_debug`.
base::Time::Exploded GetActivationTimeThreshold(bool for_debug) {
  return for_debug ? base::Time::Exploded{.year = 2024,
                                          .month = 10,
                                          .day_of_month = 1}
                   : base::Time::Exploded{
                         .year = 2024, .month = 5, .day_of_month = 28};
}

// Returns the appropriate activation URL param to use `for_debug`.
std::string GetActivationUrlParam(bool for_debug) {
  return for_debug ? "cros_standard_activation=true" : "cros_activation=true";
}

// Returns launch query params given the specified `device_info`.
std::string GetLaunchQueryParams(const std::optional<DeviceInfo>& device_info) {
  std::vector<std::string> launch_query_params;
  launch_query_params.emplace_back("cros_source=c");

  const bool for_debug =
      chromeos::features::IsContainerAppPreinstallDebugEnabled();

  // Attempt to retrieve the activation time threshold from the command-line
  // switch. Note that this switch will only be used for testing purposes.
  base::Time activation_time_threshold =
      chromeos::switches::GetContainerAppPreinstallActivationTimeThreshold()
          .value_or(base::Time());

  // Fall back to the actual activation time threshold.
  // See PRD for more information re: the threshold (http://shortn/_a762eSA1pF).
  if (activation_time_threshold.is_null()) {
    CHECK(base::Time::FromUTCExploded(GetActivationTimeThreshold(for_debug),
                                      &activation_time_threshold));
  }

  // Assume activation time is now unless that can be confirmed not to be the
  // case. This accepts the risk of a false positive to support known instances
  // where activation time may be unavailable, i.e. during first boot due to a
  // race condition between device registration and preinstallation.
  if (device_info.value_or(DeviceInfo{})
          .oobe_timestamp.value_or(base::Time::Now()) >=
      activation_time_threshold) {
    launch_query_params.emplace_back(GetActivationUrlParam(for_debug));
  }

  return base::JoinString(launch_query_params, "&");
}

}  // namespace

ExternalInstallOptions GetConfigForContainer(
    const std::optional<DeviceInfo>& device_info) {
  static constexpr char kUrl[] = "https://gemini.google.com/";
  ExternalInstallOptions options(
      /*install_url=*/GURL(kUrl),
      /*user_display_mode=*/mojom::UserDisplayMode::kStandalone,
      /*install_source=*/ExternalInstallSource::kExternalDefault);
  options.add_to_applications_menu = true;
  options.add_to_search = true;
  options.app_info_factory = base::BindRepeating(
      [](const std::optional<DeviceInfo>& device_info) {
        GURL start_url = GURL(kUrl);
        // `manifest_id` must remain fixed even if start_url changes.
        webapps::ManifestId manifest_id =
            GenerateManifestIdFromStartUrlOnly(GURL(kUrl));
        auto info = std::make_unique<WebAppInstallInfo>(manifest_id, start_url);
        info->background_color = info->theme_color = 0xFFFFFFFF;
        info->dark_mode_background_color = info->dark_mode_theme_color =
            0xFF131314;
        info->display_mode = blink::mojom::DisplayMode::kStandalone;
        info->icon_bitmaps.any = LoadBundledIcons(
            {IDR_PREINSTALLED_WEB_APPS_CONTAINER_ICON_192_PNG});
        info->launch_query_params = GetLaunchQueryParams(device_info);
        info->scope = GURL(kUrl);
        info->title = u"Gemini";
        return info;
      },
      device_info);
  options.expected_app_id = kContainerAppId;
  options.gate_on_feature = chromeos::features::kContainerAppPreinstall.name;
  options.is_preferred_app_for_supported_links = true;
  options.only_use_app_info_factory = true;
  options.user_type_allowlist = {apps::kUserTypeUnmanaged};

  // NOTE: This will cause the container app to be installed even if it was
  // previously uninstalled by the user. The container app is not intended to be
  // uninstallable. See https://crrev.com/c/chromium/src/+/5390614.
  options.override_previous_user_uninstall = true;

  return options;
}

}  // namespace web_app
