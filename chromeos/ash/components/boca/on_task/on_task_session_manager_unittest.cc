// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/on_task/on_task_session_manager.h"

#include <memory>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/boca/on_task/on_task_blocklist.h"
#include "chromeos/ash/components/boca/on_task/on_task_extensions_manager.h"
#include "chromeos/ash/components/boca/on_task/on_task_system_web_app_manager.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "components/sessions/core/session_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Sequence;

namespace ash::boca {
namespace {

constexpr char kTestUrl1[] = "https://www.test1.com";
constexpr char kTestUrl2[] = "https://www.test2.com";
constexpr char kTestUrl3[] = "https://www.test3.com";
constexpr char kTestUrl4[] = "https://www.test4.com";
constexpr char kTestUrl5[] = "https://www.test5.com";

// Mock implementation of the `OnTaskSystemWebAppManager`.
class OnTaskSystemWebAppManagerMock : public OnTaskSystemWebAppManager {
 public:
  OnTaskSystemWebAppManagerMock() = default;
  ~OnTaskSystemWebAppManagerMock() override = default;

  MOCK_METHOD(void,
              LaunchSystemWebAppAsync,
              (base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(void, CloseSystemWebAppWindow, (SessionID window_id), (override));
  MOCK_METHOD(SessionID, GetActiveSystemWebAppWindowID, (), (override));
  MOCK_METHOD(void,
              SetPinStateForSystemWebAppWindow,
              (bool pinned, SessionID window_id),
              (override));
  MOCK_METHOD(void,
              SetWindowTrackerForSystemWebAppWindow,
              (SessionID window_id, ActiveTabTracker* active_tab_tracker),
              (override));
  MOCK_METHOD(SessionID,
              CreateBackgroundTabWithUrl,
              (SessionID window_id,
               GURL url,
               OnTaskBlocklist::RestrictionLevel restriction_level),
              (override));
  MOCK_METHOD(void,
              RemoveTabsWithTabIds,
              (SessionID window_id,
               const base::flat_set<SessionID>& tab_ids_to_remove),
              (override));
};

// Mock implementation of the `OnTaskExtensionsManager`.
class OnTaskExtensionsManagerMock : public OnTaskExtensionsManager {
 public:
  OnTaskExtensionsManagerMock() = default;
  ~OnTaskExtensionsManagerMock() override = default;

  MOCK_METHOD(void, DisableExtensions, (), (override));

  MOCK_METHOD(void, ReEnableExtensions, (), (override));
};

class OnTaskSessionManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto system_web_app_manager =
        std::make_unique<NiceMock<OnTaskSystemWebAppManagerMock>>();
    system_web_app_manager_ptr_ = system_web_app_manager.get();
    auto extensions_manager =
        std::make_unique<NiceMock<OnTaskExtensionsManagerMock>>();
    extensions_manager_ptr_ = extensions_manager.get();
    session_manager_ = std::make_unique<OnTaskSessionManager>(
        std::move(system_web_app_manager), std::move(extensions_manager));
  }

  std::unique_ptr<OnTaskSessionManager> session_manager_;
  raw_ptr<NiceMock<OnTaskSystemWebAppManagerMock>> system_web_app_manager_ptr_;
  raw_ptr<NiceMock<OnTaskExtensionsManagerMock>> extensions_manager_ptr_;
};

TEST_F(OnTaskSessionManagerTest, ShouldLaunchBocaSWAOnSessionStart) {
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillRepeatedly(Return(SessionID::InvalidValue()));
  EXPECT_CALL(*system_web_app_manager_ptr_, LaunchSystemWebAppAsync(_))
      .WillOnce([](base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });
  session_manager_->OnSessionStarted("test_session_id", ::boca::UserIdentity());
}

TEST_F(OnTaskSessionManagerTest, ShouldPrepareBocaSWAOnLaunch) {
  const SessionID kWindowId = SessionID::NewUnique();
  EXPECT_CALL(*system_web_app_manager_ptr_,
              GetActiveSystemWebAppWindowID())
      .WillOnce(
          Return(SessionID::InvalidValue()))  // Initial check before launch.
      .WillOnce(Return(kWindowId));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              SetWindowTrackerForSystemWebAppWindow(
                  kWindowId, session_manager_->active_tab_tracker()))
      .Times(1);
  EXPECT_CALL(*system_web_app_manager_ptr_, LaunchSystemWebAppAsync(_))
      .WillOnce([](base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });
  session_manager_->OnSessionStarted("test_session_id", ::boca::UserIdentity());
}

TEST_F(OnTaskSessionManagerTest, ShouldClosePreExistingBocaSWAOnSessionStart) {
  const SessionID kWindowId = SessionID::NewUnique();
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillOnce(Return(kWindowId))
      .WillRepeatedly(Return(SessionID::InvalidValue()));
  EXPECT_CALL(*system_web_app_manager_ptr_, CloseSystemWebAppWindow(kWindowId))
      .Times(1);
  session_manager_->OnSessionStarted("test_session_id", ::boca::UserIdentity());
}

TEST_F(OnTaskSessionManagerTest, ShouldCloseBocaSWAOnSessionEnd) {
  const SessionID kWindowId = SessionID::NewUnique();
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillOnce(Return(kWindowId));
  EXPECT_CALL(*system_web_app_manager_ptr_, CloseSystemWebAppWindow(kWindowId))
      .Times(1);
  session_manager_->OnSessionEnded("test_session_id");
}

TEST_F(OnTaskSessionManagerTest, ShouldReEnableExtensionsOnSessionEnd) {
  const SessionID kWindowId = SessionID::NewUnique();
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillRepeatedly(Return(kWindowId));
  EXPECT_CALL(*extensions_manager_ptr_, ReEnableExtensions).Times(1);
  session_manager_->OnSessionEnded("test_session_id");
}

TEST_F(OnTaskSessionManagerTest, ShouldIgnoreWhenNoBocaSWAOpenOnSessionEnd) {
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillOnce(Return(SessionID::InvalidValue()));
  EXPECT_CALL(*system_web_app_manager_ptr_, CloseSystemWebAppWindow(_))
      .Times(0);
  session_manager_->OnSessionEnded("test_session_id");
}

TEST_F(OnTaskSessionManagerTest, ShouldOpenTabsOnBundleUpdated) {
  const SessionID kWindowId = SessionID::NewUnique();
  const SessionID kTabId_1 = SessionID::NewUnique();
  const SessionID kTabId_2 = SessionID::NewUnique();
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillRepeatedly(Return(kWindowId));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(kWindowId, GURL(kTestUrl1), _))
      .WillOnce(Return(kTabId_1));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(kWindowId, GURL(kTestUrl2), _))
      .WillOnce(Return(kTabId_2));

  ::boca::Bundle bundle;
  bundle.add_content_configs()->set_url(kTestUrl1);
  bundle.add_content_configs()->set_url(kTestUrl2);
  session_manager_->OnBundleUpdated(bundle);
}

TEST_F(OnTaskSessionManagerTest, ShouldIgnoreWhenNoBocaSWAOpenOnBundleUpdated) {
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillRepeatedly(Return(SessionID::InvalidValue()));
  EXPECT_CALL(*system_web_app_manager_ptr_, CreateBackgroundTabWithUrl(_, _, _))
      .Times(0);

  ::boca::Bundle bundle;
  bundle.add_content_configs()->set_url(kTestUrl1);
  bundle.add_content_configs()->set_url(kTestUrl2);
  session_manager_->OnBundleUpdated(bundle);
}

TEST_F(OnTaskSessionManagerTest,
       TabsCreatedAfterSWALaunchedWhenSessionStartsAndBundleUpdated) {
  const SessionID kWindowId = SessionID::NewUnique();
  const SessionID kTabId_1 = SessionID::NewUnique();
  const SessionID kTabId_2 = SessionID::NewUnique();
  Sequence s;
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillOnce(Return(
          SessionID::InvalidValue()))  // Initial check before spawning SWA
      .WillRepeatedly(Return(kWindowId));
  EXPECT_CALL(*system_web_app_manager_ptr_, LaunchSystemWebAppAsync(_))
      .InSequence(s)
      .WillOnce([](base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(kWindowId, GURL(kTestUrl1), _))
      .InSequence(s)
      .WillOnce(Return(kTabId_1));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(kWindowId, GURL(kTestUrl2), _))
      .InSequence(s)
      .WillOnce(Return(kTabId_2));

  ::boca::Bundle bundle;
  bundle.add_content_configs()->set_url(kTestUrl1);
  bundle.add_content_configs()->set_url(kTestUrl2);
  session_manager_->OnSessionStarted("test_session_id", ::boca::UserIdentity());
  session_manager_->OnBundleUpdated(bundle);
}

TEST_F(OnTaskSessionManagerTest, ShouldApplyRestrictionsToTabsOnBundleUpdated) {
  const SessionID kWindowId = SessionID::NewUnique();
  const SessionID kTabId_1 = SessionID::NewUnique();
  const SessionID kTabId_2 = SessionID::NewUnique();
  const SessionID kTabId_3 = SessionID::NewUnique();
  const SessionID kTabId_4 = SessionID::NewUnique();
  const SessionID kTabId_5 = SessionID::NewUnique();
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillRepeatedly(Return(kWindowId));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(
                  kWindowId, GURL(kTestUrl1),
                  OnTaskBlocklist::RestrictionLevel::kNoRestrictions))
      .WillOnce(Return(kTabId_1));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(
                  kWindowId, GURL(kTestUrl2),
                  OnTaskBlocklist::RestrictionLevel::kLimitedNavigation))
      .WillOnce(Return(kTabId_2));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(
                  kWindowId, GURL(kTestUrl3),
                  OnTaskBlocklist::RestrictionLevel::kSameDomainNavigation))
      .WillOnce(Return(kTabId_3));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(
                  kWindowId, GURL(kTestUrl4),
                  OnTaskBlocklist::RestrictionLevel::kOneLevelDeepNavigation))
      .WillOnce(Return(kTabId_4));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(
                  kWindowId, GURL(kTestUrl5),
                  OnTaskBlocklist::RestrictionLevel::kNoRestrictions))
      .WillOnce(Return(kTabId_5));

  ::boca::Bundle bundle;
  ::boca::ContentConfig* const content_config_1 =
      bundle.mutable_content_configs()->Add();
  content_config_1->set_url(kTestUrl1);
  content_config_1->mutable_locked_navigation_options()->set_navigation_type(
      ::boca::LockedNavigationOptions::OPEN_NAVIGATION);
  ::boca::ContentConfig* const content_config_2 =
      bundle.mutable_content_configs()->Add();
  content_config_2->set_url(kTestUrl2);
  content_config_2->mutable_locked_navigation_options()->set_navigation_type(
      ::boca::LockedNavigationOptions::BLOCK_NAVIGATION);
  ::boca::ContentConfig* const content_config_3 =
      bundle.mutable_content_configs()->Add();
  content_config_3->set_url(kTestUrl3);
  content_config_3->mutable_locked_navigation_options()->set_navigation_type(
      ::boca::LockedNavigationOptions::DOMAIN_NAVIGATION);
  ::boca::ContentConfig* const content_config_4 =
      bundle.mutable_content_configs()->Add();
  content_config_4->set_url(kTestUrl4);
  content_config_4->mutable_locked_navigation_options()->set_navigation_type(
      ::boca::LockedNavigationOptions::LIMITED_NAVIGATION);
  ::boca::ContentConfig* const content_config_5 =
      bundle.mutable_content_configs()->Add();
  content_config_5->set_url(kTestUrl5);
  session_manager_->OnBundleUpdated(bundle);
}

TEST_F(OnTaskSessionManagerTest, ShouldPinBocaSWAWhenLockedOnBundleUpdated) {
  const SessionID kWindowId = SessionID::NewUnique();
  const SessionID kTabId = SessionID::NewUnique();
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillRepeatedly(Return(kWindowId));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(kWindowId, GURL(kTestUrl1), _))
      .WillOnce(Return(kTabId));
  EXPECT_CALL(*extensions_manager_ptr_, DisableExtensions).Times(1);
  EXPECT_CALL(*system_web_app_manager_ptr_,
              SetPinStateForSystemWebAppWindow(true, kWindowId))
      .Times(1);

  ::boca::Bundle bundle;
  bundle.add_content_configs()->set_url(kTestUrl1);
  bundle.set_locked(true);
  session_manager_->OnBundleUpdated(bundle);
}

TEST_F(OnTaskSessionManagerTest, ShouldAddTabsWhenAdditionalTabsFoundInBundle) {
  const SessionID kWindowId = SessionID::NewUnique();
  const SessionID kTabId_1 = SessionID::NewUnique();
  const SessionID kTabId_2 = SessionID::NewUnique();
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillRepeatedly(Return(kWindowId));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(kWindowId, GURL(kTestUrl1), _))
      .WillOnce(Return(kTabId_1));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(kWindowId, GURL(kTestUrl2), _))
      .WillOnce(Return(kTabId_2));

  ::boca::Bundle bundle_1;
  bundle_1.add_content_configs()->set_url(kTestUrl1);
  session_manager_->OnBundleUpdated(bundle_1);

  ::boca::Bundle bundle_2;
  bundle_2.add_content_configs()->set_url(kTestUrl1);
  bundle_2.add_content_configs()->set_url(kTestUrl2);
  session_manager_->OnBundleUpdated(bundle_2);
}

TEST_F(OnTaskSessionManagerTest, ShouldRemoveTabsWhenFewerTabsFoundInBundle) {
  const SessionID kWindowId = SessionID::NewUnique();
  const SessionID kTabId_1 = SessionID::NewUnique();
  const SessionID kTabId_2 = SessionID::NewUnique();
  const base::flat_set<SessionID> kTabIds = {kTabId_2};
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillRepeatedly(Return(kWindowId));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(kWindowId, GURL(kTestUrl1), _))
      .WillOnce(Return(kTabId_1));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(kWindowId, GURL(kTestUrl2), _))
      .WillOnce(Return(kTabId_2));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              RemoveTabsWithTabIds(kWindowId, kTabIds))
      .Times(1);

  ::boca::Bundle bundle_1;
  bundle_1.add_content_configs()->set_url(kTestUrl1);
  bundle_1.add_content_configs()->set_url(kTestUrl2);
  session_manager_->OnBundleUpdated(bundle_1);

  ::boca::Bundle bundle_2;
  bundle_2.add_content_configs()->set_url(kTestUrl1);
  session_manager_->OnBundleUpdated(bundle_2);
}

TEST_F(OnTaskSessionManagerTest, ShouldDisableExtensionsOnLock) {
  const SessionID kWindowId = SessionID::NewUnique();
  const SessionID kTabId = SessionID::NewUnique();
  Sequence s;
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillRepeatedly(Return(kWindowId));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(kWindowId, GURL(kTestUrl1), _))
      .InSequence(s)
      .WillOnce(Return(kTabId));
  EXPECT_CALL(*extensions_manager_ptr_, DisableExtensions)
      .Times(1)
      .InSequence(s);
  EXPECT_CALL(*system_web_app_manager_ptr_,
              SetPinStateForSystemWebAppWindow(true, kWindowId))
      .Times(1)
      .InSequence(s);

  ::boca::Bundle bundle;
  bundle.add_content_configs()->set_url(kTestUrl1);
  bundle.set_locked(true);
  session_manager_->OnBundleUpdated(bundle);
}

TEST_F(OnTaskSessionManagerTest, ShouldReEnableExtensionsOnUnlock) {
  const SessionID kWindowId = SessionID::NewUnique();
  const SessionID kTabId = SessionID::NewUnique();
  Sequence s;
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillRepeatedly(Return(kWindowId));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(kWindowId, GURL(kTestUrl1), _))
      .InSequence(s)
      .WillOnce(Return(kTabId));
  EXPECT_CALL(*extensions_manager_ptr_, ReEnableExtensions)
      .Times(1)
      .InSequence(s);
  EXPECT_CALL(*system_web_app_manager_ptr_,
              SetPinStateForSystemWebAppWindow(false, kWindowId))
      .Times(1)
      .InSequence(s);

  ::boca::Bundle bundle;
  bundle.add_content_configs()->set_url(kTestUrl1);
  bundle.set_locked(false);
  session_manager_->OnBundleUpdated(bundle);
}

}  // namespace
}  // namespace ash::boca
