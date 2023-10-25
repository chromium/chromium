// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_ISOLATED_WEB_APP_TEST_UTILS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_ISOLATED_WEB_APP_TEST_UTILS_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "base/version.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/web_app.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/window_open_disposition.h"

class Browser;
class GURL;
class Profile;

namespace content {
class RenderFrameHost;
}  // namespace content

namespace net::test_server {
class EmbeddedTestServer;
}  // namespace net::test_server

namespace url {
class Origin;
}  // namespace url

namespace web_app {

class IsolatedWebAppUrlInfo;

class IsolatedWebAppBrowserTestHarness : public WebAppControllerBrowserTest {
 public:
  IsolatedWebAppBrowserTestHarness();
  IsolatedWebAppBrowserTestHarness(const IsolatedWebAppBrowserTestHarness&) =
      delete;
  IsolatedWebAppBrowserTestHarness& operator=(
      const IsolatedWebAppBrowserTestHarness&) = delete;
  ~IsolatedWebAppBrowserTestHarness() override;

 protected:
  std::unique_ptr<net::EmbeddedTestServer> CreateAndStartServer(
      const base::FilePath::StringPieceType& chrome_test_data_relative_root);
  IsolatedWebAppUrlInfo InstallDevModeProxyIsolatedWebApp(
      const url::Origin& origin);
  content::RenderFrameHost* OpenApp(const webapps::AppId& app_id,
                                    base::StringPiece path = "");
  content::RenderFrameHost* NavigateToURLInNewTab(
      Browser* window,
      const GURL& url,
      WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB);

  Browser* GetBrowserFromFrame(content::RenderFrameHost* frame);

 private:
  base::test::ScopedFeatureList iwa_scoped_feature_list_;
};

std::unique_ptr<net::EmbeddedTestServer> CreateAndStartDevServer(
    const base::FilePath::StringPieceType& chrome_test_data_relative_root);

IsolatedWebAppUrlInfo InstallDevModeProxyIsolatedWebApp(
    Profile* profile,
    const url::Origin& proxy_origin);

content::RenderFrameHost* OpenIsolatedWebApp(Profile* profile,
                                             const webapps::AppId& app_id,
                                             base::StringPiece path = "");

void CreateIframe(content::RenderFrameHost* parent_frame,
                  const std::string& iframe_id,
                  const GURL& url,
                  const std::string& permissions_policy);

// Adds an Isolated Web App to the WebAppRegistrar. The IWA will have an empty
// filepath for |IsolatedWebAppLocation|.
webapps::AppId AddDummyIsolatedAppToRegistry(
    Profile* profile,
    const GURL& start_url,
    const std::string& name,
    const WebApp::IsolationData& isolation_data =
        WebApp::IsolationData(InstalledBundle{.path = base::FilePath()},
                              base::Version("1.0.0")));

// TODO(cmfcmf): Move more test utils into this `test` namespace
namespace test {

using ::testing::AllOf;
using ::testing::ExplainMatchResult;
using ::testing::Field;
using ::testing::Optional;
using ::testing::Pointee;
using ::testing::Property;

MATCHER_P(IsInIwaRandomDir, profile_directory, "") {
  *result_listener << "where the profile directory is " << profile_directory;
  return arg.DirName().DirName() == profile_directory.Append(kIwaDirName) &&
         arg.BaseName() == base::FilePath(kMainSwbnFileName);
}

MATCHER_P2(IwaIs, untranslated_name, isolation_data, "") {
  return ExplainMatchResult(
      Pointee(AllOf(
          Property("untranslated_name", &WebApp::untranslated_name,
                   untranslated_name),
          Property("isolation_data", &WebApp::isolation_data, isolation_data))),
      arg, result_listener);
}

MATCHER_P4(IsolationDataIs,
           location,
           version,
           controlled_frame_partitions,
           pending_update_info,
           "") {
  return ExplainMatchResult(
      Optional(
          AllOf(Field("location", &WebApp::IsolationData::location, location),
                Field("version", &WebApp::IsolationData::version, version),
                Field("controlled_frame_partitions",
                      &WebApp::IsolationData::controlled_frame_partitions,
                      controlled_frame_partitions),
                Property("pending_update_info",
                         &WebApp::IsolationData::pending_update_info,
                         pending_update_info))),
      arg, result_listener);
}

MATCHER_P2(PendingUpdateInfoIs, location, version, "") {
  return ExplainMatchResult(
      Optional(AllOf(
          Field("location", &WebApp::IsolationData::PendingUpdateInfo::location,
                location),
          Field("version", &WebApp::IsolationData::PendingUpdateInfo::version,
                version))),
      arg, result_listener);
}

std::string BitmapAsPng(const SkBitmap& bitmap);

}  // namespace test

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_ISOLATED_WEB_APP_TEST_UTILS_H_
