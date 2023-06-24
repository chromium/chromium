// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_PENDING_INSTALL_INFO_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_PENDING_INSTALL_INFO_H_

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class WebContents;
}

namespace web_app {

// Indicates that the specific instance of |WebContents| serves data for IWA
// installation. Components which share the same instance of |WebContents| can
// read installation info data.
class IsolatedWebAppPendingInstallInfo {
 public:
  static IsolatedWebAppPendingInstallInfo& FromWebContents(
      content::WebContents& web_contents);

  IsolatedWebAppPendingInstallInfo(const IsolatedWebAppPendingInstallInfo&) =
      delete;
  IsolatedWebAppPendingInstallInfo& operator=(
      const IsolatedWebAppPendingInstallInfo&) = delete;
  IsolatedWebAppPendingInstallInfo(IsolatedWebAppPendingInstallInfo&&) = delete;
  IsolatedWebAppPendingInstallInfo& operator=(
      IsolatedWebAppPendingInstallInfo&&) = delete;

  ~IsolatedWebAppPendingInstallInfo();

  void set_isolated_web_app_location(const IsolatedWebAppLocation& location);

  const absl::optional<IsolatedWebAppLocation>& location() const;

  void ResetIsolatedWebAppLocation();

 private:
  IsolatedWebAppPendingInstallInfo();

  absl::optional<IsolatedWebAppLocation> location_ = absl::nullopt;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_PENDING_INSTALL_INFO_H_
