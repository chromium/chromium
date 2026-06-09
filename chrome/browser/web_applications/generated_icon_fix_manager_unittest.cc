// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/generated_icon_fix_manager.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/generated_icon_fix_util.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

class MockNetworkConnectionTracker : public network::NetworkConnectionTracker {
 public:
  MockNetworkConnectionTracker() = default;
  MockNetworkConnectionTracker(const MockNetworkConnectionTracker&) = delete;
  MockNetworkConnectionTracker& operator=(const MockNetworkConnectionTracker&) =
      delete;
  ~MockNetworkConnectionTracker() override = default;

  MOCK_METHOD(bool,
              GetConnectionType,
              (net::NetworkChangeNotifier::ConnectionType*,
               network::NetworkConnectionTracker::ConnectionTypeCallback),
              (override));

  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override {
    network::NetworkConnectionTracker::OnNetworkChanged(type);
  }
};

class GeneratedIconFixManagerTest : public WebAppTest {
 public:
  GeneratedIconFixManagerTest()
      : WebAppTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    mock_tracker_ =
        std::make_unique<testing::NiceMock<MockNetworkConnectionTracker>>();
    content::SetNetworkConnectionTrackerForTesting(nullptr);
    content::SetNetworkConnectionTrackerForTesting(mock_tracker_.get());
    disable_auto_retry_ = std::make_unique<base::AutoReset<bool>>(
        GeneratedIconFixManager::DisableAutoRetryForTesting());

    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void TearDown() override {
    WebAppTest::TearDown();
    disable_auto_retry_.reset();
    content::SetNetworkConnectionTrackerForTesting(nullptr);
    mock_tracker_.reset();
  }

  webapps::AppId InstallSyncedAppWithGeneratedIcon() {
    auto install_info = std::make_unique<WebAppInstallInfo>(
        GenerateManifestIdFromStartUrlOnly(GURL("https://example.com/app/")),
        GURL("https://example.com/app/"));
    install_info->title = u"Test App";

    webapps::AppId app_id =
        test::InstallWebApp(profile(), std::move(install_info));
    ScopedRegistryUpdate update = provider().sync_bridge_unsafe().BeginUpdate();
    WebApp* app = update->UpdateApp(app_id);
    app->AddSource(WebAppManagement::kSync);
    app->SetGeneratedIconFix(generated_icon_fix_util::CreateInitialTimeWindow(
        proto::GENERATED_ICON_FIX_SOURCE_SYNC_INSTALL));
    CHECK(app->is_generated_icon());
    return app_id;
  }

  MockNetworkConnectionTracker& mock_tracker() { return *mock_tracker_; }

 private:
  using NiceMockNetworkConnectionTracker =
      testing::NiceMock<MockNetworkConnectionTracker>;

  std::unique_ptr<NiceMockNetworkConnectionTracker> mock_tracker_;
  std::unique_ptr<base::AutoReset<bool>> disable_auto_retry_;
};

TEST_F(GeneratedIconFixManagerTest, SyncUpdateSchedulesDelayedFix) {
  webapps::AppId app_id = InstallSyncedAppWithGeneratedIcon();

  base::test::TestFuture<const webapps::AppId&,
                         GeneratedIconFixScheduleDecision>
      schedule_future;
  provider()
      .generated_icon_fix_manager()
      .maybe_schedule_callback_for_testing() = schedule_future.GetCallback();

  const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
  provider().registrar_unsafe().NotifyWebAppsWillBeUpdatedFromSync({app});
  task_environment()->FastForwardBy(base::Minutes(10));

  EXPECT_EQ(schedule_future.Get<0>(), app_id);
  EXPECT_EQ(schedule_future.Get<1>(),
            GeneratedIconFixScheduleDecision::kSchedule);
}

TEST_F(GeneratedIconFixManagerTest, NetworkReconnectSchedulesFix) {
  webapps::AppId app_id = InstallSyncedAppWithGeneratedIcon();

  base::test::TestFuture<const webapps::AppId&,
                         GeneratedIconFixScheduleDecision>
      schedule_future;
  provider()
      .generated_icon_fix_manager()
      .maybe_schedule_callback_for_testing() = schedule_future.GetCallback();

  mock_tracker().OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI);

  EXPECT_EQ(schedule_future.Get<0>(), app_id);
  EXPECT_EQ(schedule_future.Get<1>(),
            GeneratedIconFixScheduleDecision::kSchedule);
}

}  // namespace
}  // namespace web_app
