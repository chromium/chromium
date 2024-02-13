// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/banners/install_banner_config.h"

namespace webapps {

InstallBannerConfig::InstallBannerConfig(
    GURL validated_url,
    AppBannerMode app_mode,
    const WebAppBannerData& web_app_data,
    const std::optional<NativeAppBannerData>& native_app_data)
    : validated_url(std::move(validated_url)),
      mode(app_mode),
      web_app_data(web_app_data),
      native_app_data(native_app_data) {}
InstallBannerConfig::InstallBannerConfig(const InstallBannerConfig& other) =
    default;
InstallBannerConfig::~InstallBannerConfig() = default;

std::string InstallBannerConfig::GetWebOrNativeAppIdentifier() const {
  switch (mode) {
    case AppBannerMode::kNativeApp:
      CHECK(native_app_data);
      return native_app_data->app_package;
    case AppBannerMode::kWebApp:
      return web_app_data.manifest_id.spec();
  }
}

// Returns the name of either the web app or the native app, depending on the
// `mode`.
std::u16string InstallBannerConfig::GetWebOrNativeAppName() const {
  switch (mode) {
    case AppBannerMode::kNativeApp:
      CHECK(native_app_data);
      return native_app_data->app_title;
    case AppBannerMode::kWebApp:
      return web_app_data.GetAppName();
  }
}

}  // namespace webapps
