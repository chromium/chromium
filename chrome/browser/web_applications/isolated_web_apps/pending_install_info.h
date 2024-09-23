// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_PENDING_INSTALL_INFO_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_PENDING_INSTALL_INFO_H_

#include <optional>

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_source.h"

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

  void set_source(const IwaSourceWithMode& source);

  const std::optional<IwaSourceWithMode>& source() const;

  void ResetSource();

 private:
  IsolatedWebAppPendingInstallInfo();

  std::optional<IwaSourceWithMode> source_ = std::nullopt;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_PENDING_INSTALL_INFO_H_
