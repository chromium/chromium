// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_ISOLATED_WEB_APP_TEST_UTILS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_ISOLATED_WEB_APP_TEST_UTILS_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/version.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/version_info/channel.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/isolated_web_apps/types/source.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "extensions/common/features/feature_channel.h"
#include "testing/gmock/include/gmock/gmock.h"
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

class IsolatedWebAppBrowserTestHarness : public WebAppBrowserTestBase {
 public:
  IsolatedWebAppBrowserTestHarness();
  IsolatedWebAppBrowserTestHarness(const IsolatedWebAppBrowserTestHarness&) =
      delete;
  IsolatedWebAppBrowserTestHarness& operator=(
      const IsolatedWebAppBrowserTestHarness&) = delete;
  ~IsolatedWebAppBrowserTestHarness() override;

 protected:
  std::unique_ptr<net::EmbeddedTestServer> CreateAndStartServer(
      base::FilePath::StringViewType chrome_test_data_relative_root);

  // Overrides the Key Distribution component. Sets managed allowlist of bundle
  // ids which enables installing and updating Isolated Web Apps from policy.
  // CAUTION: This function fully overrides all fields of the Key Distribution
  // component (see IwaKeyDistributionInfoProvider::ComponentData), take care to
  // not override other fields important for a testing feature or previously
  // allowlisted bundles. CAUTION: Subsequent calls must use a higher
  // `component_version`.
  void SetIwaManagedAllowlist(
      const std::vector<web_package::SignedWebBundleId>& managed_allowlist,
      const base::Version& component_version);

  IsolatedWebAppUrlInfo InstallDevModeProxyIsolatedWebApp(
      const url::Origin& origin);
  content::RenderFrameHost* OpenApp(
      const webapps::AppId& app_id,
      std::optional<std::string_view> path = std::nullopt);
  content::RenderFrameHost* NavigateToURLInNewTab(
      Browser* window,
      const GURL& url,
      WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB);

  Browser* GetBrowserFromFrame(content::RenderFrameHost* frame);

 private:
  base::test::ScopedFeatureList iwa_scoped_feature_list_;
  // Various IsolatedWebAppBrowsing tests fail on official builds because
  // stable channel doesn't enable a required feature.
  // TODO(b/309153867): Remove this when underlying issue is figured out.
  extensions::ScopedCurrentChannel channel_{version_info::Channel::CANARY};
};

class UpdateDiscoveryTaskResultWaiter
    : public IsolatedWebAppUpdateManager::Observer {
  using TaskResultCallback = base::OnceCallback<void(
      IsolatedWebAppUpdateDiscoveryTask::CompletionStatus status)>;

 public:
  UpdateDiscoveryTaskResultWaiter(WebAppProvider& provider,
                                  const webapps::AppId expected_app_id,
                                  TaskResultCallback callback);
  ~UpdateDiscoveryTaskResultWaiter() override;

  // IsolatedWebAppUpdateManager::Observer:
  void OnUpdateDiscoveryTaskCompleted(
      const webapps::AppId& app_id,
      IsolatedWebAppUpdateDiscoveryTask::CompletionStatus status) override;

 private:
  const webapps::AppId expected_app_id_;
  TaskResultCallback callback_;
  const raw_ref<WebAppProvider> provider_;

  base::ScopedObservation<IsolatedWebAppUpdateManager,
                          IsolatedWebAppUpdateManager::Observer>
      observation_{this};
};

class UpdateApplyTaskResultWaiter
    : public IsolatedWebAppUpdateManager::Observer {
  using TaskResultCallback =
      base::OnceCallback<void(IsolatedWebAppApplyUpdateCommandResult status)>;

 public:
  UpdateApplyTaskResultWaiter(WebAppProvider& provider,
                              const webapps::AppId expected_app_id,
                              TaskResultCallback callback);
  ~UpdateApplyTaskResultWaiter() override;

  // IsolatedWebAppUpdateManager::Observer:
  void OnUpdateApplyTaskCompleted(
      const webapps::AppId& app_id,
      IsolatedWebAppApplyUpdateCommandResult status) override;

 private:
  const webapps::AppId expected_app_id_;
  TaskResultCallback callback_;
  const raw_ref<WebAppProvider> provider_;

  base::ScopedObservation<IsolatedWebAppUpdateManager,
                          IsolatedWebAppUpdateManager::Observer>
      observation_{this};
};

std::unique_ptr<net::EmbeddedTestServer> CreateAndStartDevServer(
    base::FilePath::StringViewType chrome_test_data_relative_root);

IsolatedWebAppUrlInfo InstallDevModeProxyIsolatedWebApp(
    Profile* profile,
    const url::Origin& proxy_origin);

content::RenderFrameHost* OpenIsolatedWebApp(
    Profile* profile,
    const webapps::AppId& app_id,
    std::optional<std::string_view> path = std::nullopt);

void CreateIframe(content::RenderFrameHost* parent_frame,
                  const std::string& iframe_id,
                  const GURL& url,
                  const std::string& permissions_policy);

// Simulates navigating `web_contents` main frame to the provided isolated-app:
// URL for unit tests. `TestWebContents::NavigateAndCommit` won't work for IWAs
// because they require COI headers, but the IsolatedWebAppURLLoaderFactory
// that injects them isn't run in RenderViewHostTestHarness-based unit tests.
void SimulateIsolatedWebAppNavigation(content::WebContents* web_contents,
                                      const GURL& url);

// Commits a pending IWA navigation in `web_contents`. This should be called
// instead of `RenderFrameHostTester::CommitPendingLoad` in IWAs because COI
// headers need to be injected.
void CommitPendingIsolatedWebAppNavigation(content::WebContents* web_contents);

// TODO(cmfcmf): Move more test utils into this `test` namespace
namespace test {
namespace {
using ::testing::AllOf;
using ::testing::ExplainMatchResult;
using ::testing::Field;
using ::testing::Optional;
using ::testing::Pointee;
using ::testing::Property;
}  // namespace

MATCHER_P(IsInIwaRandomDir, profile_directory, "") {
  *result_listener << "where the profile directory is " << profile_directory;
  return arg.DirName().DirName() == profile_directory.Append(kIwaDirName) &&
         arg.BaseName() == base::FilePath(kMainSwbnFileName);
}

MATCHER(FileExists, "") {
  base::ScopedAllowBlockingForTesting allow_blocking;
  return base::PathExists(arg) && !base::DirectoryExists(arg);
}

MATCHER_P(OwnedIwaBundleExists, profile_directory, "") {
  *result_listener << "where the profile directory is " << profile_directory;
  base::FilePath path = arg.GetPath(profile_directory);
  return ExplainMatchResult(
      AllOf(IsInIwaRandomDir(profile_directory), FileExists()), path,
      result_listener);
}

MATCHER_P2(IwaIs, untranslated_name, isolation_data, "") {
  return ExplainMatchResult(
      Pointee(AllOf(
          Property("untranslated_name", &WebApp::untranslated_name,
                   untranslated_name),
          Property("isolation_data", &WebApp::isolation_data, isolation_data))),
      arg, result_listener);
}

MATCHER_P5(IsolationDataIs,
           location,
           version,
           controlled_frame_partitions,
           pending_update_info,
           integrity_block_data,
           "") {
  return ExplainMatchResult(
      Optional(AllOf(
          Property("location", &IsolationData::location, location),
          Property("version", &IsolationData::version, version),
          Property("controlled_frame_partitions",
                   &IsolationData::controlled_frame_partitions,
                   controlled_frame_partitions),
          Property("pending_update_info", &IsolationData::pending_update_info,
                   pending_update_info),
          Property("integrity_block_data", &IsolationData::integrity_block_data,
                   integrity_block_data))),
      arg, result_listener);
}

MATCHER_P3(PendingUpdateInfoIs, location, version, integrity_block_data, "") {
  return ExplainMatchResult(
      Optional(AllOf(
          Field("location", &IsolationData::PendingUpdateInfo::location,
                location),
          Field("version", &IsolationData::PendingUpdateInfo::version, version),
          Field("integrity_block_data",
                &IsolationData::PendingUpdateInfo::integrity_block_data,
                integrity_block_data))),
      arg, result_listener);
}

}  // namespace test

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_ISOLATED_WEB_APP_TEST_UTILS_H_
