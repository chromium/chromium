// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_BANNERS_INSTALL_BANNER_CONFIG_H_
#define COMPONENTS_WEBAPPS_BROWSER_BANNERS_INSTALL_BANNER_CONFIG_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "components/webapps/browser/banners/native_app_banner_data.h"
#include "components/webapps/browser/banners/web_app_banner_data.h"
#include "url/gurl.h"

namespace webapps {

enum class AppBannerMode { kWebApp, kNativeApp };

struct InstallBannerConfig {
  InstallBannerConfig() = delete;
  InstallBannerConfig(
      GURL validated_url,
      AppBannerMode app_mode,
      const WebAppBannerData& web_app_data,
      const std::optional<NativeAppBannerData>& native_app_data);
  InstallBannerConfig(const InstallBannerConfig&);
  ~InstallBannerConfig();

  // Returns the identifier of either the web app or the native app, depending
  // on the `mode`.
  std::string GetWebOrNativeAppIdentifier() const;

  // Returns the name of either the web app or the native app, depending on the
  // `mode`.
  std::u16string GetWebOrNativeAppName() const;

  const GURL validated_url;
  const AppBannerMode mode;
  const WebAppBannerData web_app_data;
  const std::optional<NativeAppBannerData> native_app_data;
};

using InstallBannerConfigCallback =
    base::OnceCallback<void(std::optional<InstallBannerConfig> result)>;
}  // namespace webapps
#endif  // COMPONENTS_WEBAPPS_BROWSER_BANNERS_INSTALL_BANNER_CONFIG_H_
