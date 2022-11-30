// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_PENDING_INSTALL_INFO_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_PENDING_INSTALL_INFO_H_

#include "chrome/browser/web_applications/isolation_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class WebContents;
}

namespace web_app {

struct IsolationData;

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

  void set_isolation_data(const IsolationData& isolation_data);

  const absl::optional<IsolationData>& isolation_data() const;

  void ResetIsolationData();

 private:
  IsolatedWebAppPendingInstallInfo();

  absl::optional<IsolationData> isolation_data_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_PENDING_INSTALL_INFO_H_
