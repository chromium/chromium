// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_source.h"

#include "base/files/file_path.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace web_app {

TEST(IsolatedWebAppInstallSourceTest, FromGraphicalInstaller) {
  auto install_source = IsolatedWebAppInstallSource::FromGraphicalInstaller(
      IwaSourceBundleProdModeWithFileOp(base::FilePath(),
                                        IwaSourceBundleProdFileOp::kMove));
  EXPECT_EQ(install_source.install_surface(),
            webapps::WebappInstallSource::IWA_GRAPHICAL_INSTALLER);
}

TEST(IsolatedWebAppInstallSourceTest, FromExternalPolicy) {
  auto install_source = IsolatedWebAppInstallSource::FromExternalPolicy(
      IwaSourceBundleProdModeWithFileOp(base::FilePath(),
                                        IwaSourceBundleProdFileOp::kCopy));
  EXPECT_EQ(install_source.install_surface(),
            webapps::WebappInstallSource::IWA_EXTERNAL_POLICY);
}

TEST(IsolatedWebAppInstallSourceTest, FromShimlessRma) {
  auto install_source = IsolatedWebAppInstallSource::FromShimlessRma(
      IwaSourceBundleProdModeWithFileOp(base::FilePath(),
                                        IwaSourceBundleProdFileOp::kMove));
  EXPECT_EQ(install_source.install_surface(),
            webapps::WebappInstallSource::IWA_SHIMLESS_RMA);
}

TEST(IsolatedWebAppInstallSourceTest, FromDevUi) {
  auto install_source =
      IsolatedWebAppInstallSource::FromDevUi(IwaSourceProxy(url::Origin()));
  EXPECT_EQ(install_source.install_surface(),
            webapps::WebappInstallSource::IWA_DEV_UI);
}

TEST(IsolatedWebAppInstallSourceTest, FromDevCommandLine) {
  auto install_source = IsolatedWebAppInstallSource::FromDevCommandLine(
      IwaSourceProxy(url::Origin()));
  EXPECT_EQ(install_source.install_surface(),
            webapps::WebappInstallSource::IWA_DEV_COMMAND_LINE);
}

}  // namespace web_app
