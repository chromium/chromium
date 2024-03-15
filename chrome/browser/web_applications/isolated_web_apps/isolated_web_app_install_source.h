// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALL_SOURCE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALL_SOURCE_H_

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_source.h"

namespace base {
class Value;
}  // namespace base

namespace webapps {
enum class WebappInstallSource;
}

namespace web_app {

class IsolatedWebAppInstallSource {
 public:
  static IsolatedWebAppInstallSource FromGraphicalInstaller(
      IwaSourceBundleWithModeAndFileOp source);

  static IsolatedWebAppInstallSource FromExternalPolicy(
      IwaSourceProdModeWithFileOp source);

  static IsolatedWebAppInstallSource FromShimlessRma(
      IwaSourceProdModeWithFileOp source);

  static IsolatedWebAppInstallSource FromDevUi(
      IwaSourceDevModeWithFileOp source);

  static IsolatedWebAppInstallSource FromDevCommandLine(
      IwaSourceDevModeWithFileOp source);

  IsolatedWebAppInstallSource(const IsolatedWebAppInstallSource&);
  IsolatedWebAppInstallSource& operator=(const IsolatedWebAppInstallSource&);

  ~IsolatedWebAppInstallSource();

  const IwaSourceWithModeAndFileOp& source() const { return source_; }

  webapps::WebappInstallSource install_surface() const {
    return install_surface_;
  }

  base::Value ToDebugValue() const;

 private:
  IsolatedWebAppInstallSource(IwaSourceWithModeAndFileOp source,
                              webapps::WebappInstallSource install_surface);

  IwaSourceWithModeAndFileOp source_;
  webapps::WebappInstallSource install_surface_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALL_SOURCE_H_
