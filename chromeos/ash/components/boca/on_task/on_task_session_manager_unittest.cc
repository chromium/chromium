// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/on_task/on_task_session_manager.h"

#include <memory>
#include <optional>
#include <set>
#include <utility>

#include "ash/public/cpp/system/toast_data.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/task/current_thread.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/boca/on_task/activity/active_tab_tracker.h"
#include "chromeos/ash/components/boca/on_task/notification_constants.h"
#include "chromeos/ash/components/boca/on_task/on_task_blocklist.h"
#include "chromeos/ash/components/boca/on_task/on_task_extensions_manager.h"
#include "chromeos/ash/components/boca/on_task/on_task_notifications_manager.h"
#include "chromeos/ash/components/boca/on_task/on_task_system_web_app_manager.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "components/sessions/core/session_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::UnorderedElementsAreArray;

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
              (SessionID window_id,
               const std::vector<boca::BocaWindowObserver*> observers),
              (override));
  MOCK_METHOD(
      SessionID,
      CreateBackgroundTabWithUrl,
      (SessionID window_id,
       GURL url,
       ::boca::LockedNavigationOptions::NavigationType restriction_level),
      (override));
  MOCK_METHOD(void,
              RemoveTabsWithTabIds,
              (SessionID window_id,
               const std::set<SessionID>& tab_ids_to_remove),
              (override));
  MOCK_METHOD(void,
              PrepareSystemWebAppWindowForOnTask,
              (SessionID window_id),
              (override));
  MOCK_METHOD(SessionID, GetActiveTabID, (), (override));
  MOCK_METHOD(void, SwitchToTab, (SessionID tab_id), (override));
};

// Mock implementation of the `OnTaskExtensionsManager`.
class OnTaskExtensionsManagerMock : public OnTaskExtensionsManager {
 public:
  OnTaskExtensionsManagerMock() = default;
  ~OnTaskExtensionsManagerMock() override = default;

  MOCK_METHOD(void, DisableExtensions, (), (override));

  MOCK_METHOD(void, ReEnableExtensions, (), (override));
};

// Fake delegate implementation for the `OnTaskNotificationsManager` to minimize
// dependency on Ash UI.
class FakeOnTaskNotificationsManagerDelegate
    : public OnTaskNotificationsManager::Delegate {
 public:
  FakeOnTaskNotificationsManagerDelegate() = default;
  ~FakeOnTaskNotificationsManagerDelegate() override = default;

  // OnTaskNotificationsManager::Delegate:
  void ShowNotification(
      std::unique_ptr<message_center::Notification> notification) override {
    notifications_shown_.insert(notification->id());
  }
  void ClearNotification(const std::string& notification_id) override {
    notifications_shown_.erase(notification_id);
  }

  bool WasNotificationShown(const std::string& id) {
    return notifications_shown_.contains(id);
  }

 private:
  std::set<std::string> notifications_shown_;
};

}  // namespace

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

    // Override notification manager implementation to minimize dependency on
    // Ash UI.
    auto fake_notifications_delegate =
        std::make_unique<FakeOnTaskNotificationsManagerDelegate>();
    fake_notifications_delegate_ptr_ = fake_notifications_delegate.get();
    session_manager_->notifications_manager_ =
        OnTaskNotificationsManager::CreateForTest(
            std::move(fake_notifications_delegate));
  }

  base::flat_map<GURL, std::set<SessionID>>* provider_url_tab_ids_map() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(session_manager_->sequence_checker_);
    return &session_manager_->provider_url_tab_ids_map_;
  }

  base::flat_map<GURL, ::boca::LockedNavigationOptions::NavigationType>*
  provider_url_restriction_level_map() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(session_manager_->sequence_checker_);
    return &session_manager_->provider_url_restriction_level_map_;
  }

  std::optional<std::string>* active_session_id() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(session_manager_->sequence_checker_);
    return &session_manager_->active_session_id_;
  }

  bool* should_lock_window() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(session_manager_->sequence_checker_);
    return &session_manager_->should_lock_window_;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<OnTaskSessionManager> session_manager_;
  raw_ptr<NiceMock<OnTaskSystemWebAppManagerMock>> system_web_app_manager_ptr_;
  raw_ptr<NiceMock<OnTaskExtensionsManagerMock>> extensions_manager_ptr_;
  raw_ptr<FakeOnTaskNotificationsManagerDelegate>
      fake_notifications_delegate_ptr_;
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
  const std::vector<boca::BocaWindowObserver*> kWindowObservers = {
      session_manager_->active_tab_tracker(), session_manager_.get()};
  const SessionID kWindowId = SessionID::NewUnique();
  EXPECT_CALL(*system_web_app_manager_ptr_,
              GetActiveSystemWebAppWindowID())
      .WillOnce(
          Return(SessionID::InvalidValue()))  // Initial check before launch.
      .WillOnce(Return(kWindowId));
  EXPECT_CALL(
      *system_web_app_manager_ptr_,
      SetWindowTrackerForSystemWebAppWindow(kWindowId, kWindowObservers))
      .Times(1);
  EXPECT_CALL(*system_web_app_manager_ptr_, LaunchSystemWebAppAsync(_))
      .WillOnce([](base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });
  session_manager_->OnSessionStarted("test_session_id", ::boca::UserIdentity());
}

TEST_F(OnTaskSessionManagerTest,
       ShouldPreparePreExistingBocaSWAOnSessionStart) {
  const SessionID kWindowId = SessionID::NewUnique();
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillOnce(Return(kWindowId));

  const std::vector<boca::BocaWindowObserver*> kWindowObservers = {
      session_manager_->active_tab_tracker(), session_manager_.get()};
  Sequence s;
  EXPECT_CALL(*system_web_app_manager_ptr_,
              PrepareSystemWebAppWindowForOnTask(kWindowId))
      .Times(1)
      .InSequence(s);
  EXPECT_CALL(
      *system_web_app_manager_ptr_,
      SetWindowTrackerForSystemWebAppWindow(kWindowId, kWindowObservers))
      .Times(1)
      .InSequence(s);
  session_manager_->OnSessionStarted("test_session_id", ::boca::UserIdentity());
}

TEST_F(OnTaskSessionManagerTest, ShouldCloseBocaSWAOnSessionEnd) {
  const SessionID kWindowId = SessionID::NewUnique();
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillOnce(Return(kWindowId));
  EXPECT_CALL(*system_web_app_manager_ptr_, CloseSystemWebAppWindow(kWindowId))
      .Times(1);
  session_manager_->OnSessionEnded("test_session_id");

  // Verify session end notification was shown and window lock state was reset.
  EXPECT_TRUE(fake_notifications_delegate_ptr_->WasNotificationShown(
      kOnTaskSessionEndNotificationId));
  EXPECT_FALSE(*should_lock_window());
}

TEST_F(OnTaskSessionManagerTest, ShouldReEnableExtensionsOnSessionEnd) {
  const SessionID kWindowId = SessionID::NewUnique();
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillRepeatedly(Return(kWindowId));
  EXPECT_CALL(*extensions_manager_ptr_, ReEnableExtensions).Times(1);
  session_manager_->OnSessionEnded("test_session_id");

  // Verify session end notification was shown.
  EXPECT_TRUE(fake_notifications_delegate_ptr_->WasNotificationShown(
      kOnTaskSessionEndNotificationId));
}

TEST_F(OnTaskSessionManagerTest, ShouldIgnoreWhenNoBocaSWAOpenOnSessionEnd) {
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillOnce(Return(SessionID::InvalidValue()));
  EXPECT_CALL(*system_web_app_manager_ptr_, CloseSystemWebAppWindow(_))
      .Times(0);
  session_manager_->OnSessionEnded("test_session_id");

  // Verify session end notification was shown.
  EXPECT_TRUE(fake_notifications_delegate_ptr_->WasNotificationShown(
      kOnTaskSessionEndNotificationId));
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

  // Verify that relevant notification is shown.
  EXPECT_TRUE(fake_notifications_delegate_ptr_->WasNotificationShown(
      kOnTaskBundleContentAddedNotificationId));
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
                  ::boca::LockedNavigationOptions::OPEN_NAVIGATION))
      .WillOnce(Return(kTabId_1));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(
                  kWindowId, GURL(kTestUrl2),
                  ::boca::LockedNavigationOptions::BLOCK_NAVIGATION))
      .WillOnce(Return(kTabId_2));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(
                  kWindowId, GURL(kTestUrl3),
                  ::boca::LockedNavigationOptions::DOMAIN_NAVIGATION))
      .WillOnce(Return(kTabId_3));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(
                  kWindowId, GURL(kTestUrl4),
                  ::boca::LockedNavigationOptions::LIMITED_NAVIGATION))
      .WillOnce(Return(kTabId_4));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(
                  kWindowId, GURL(kTestUrl5),
                  ::boca::LockedNavigationOptions::OPEN_NAVIGATION))
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

  // Verify notification is shown.
  EXPECT_TRUE(fake_notifications_delegate_ptr_->WasNotificationShown(
      kOnTaskEnterLockedModeNotificationId));
}

TEST_F(OnTaskSessionManagerTest,
       ShouldNotShowNotificationIfWindowWasAlreadyLocked) {
  // Set previous window locked state for testing purposes.
  *should_lock_window() = true;

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

  // Verify notification is not shown.
  EXPECT_FALSE(fake_notifications_delegate_ptr_->WasNotificationShown(
      kOnTaskEnterLockedModeNotificationId));
}

TEST_F(OnTaskSessionManagerTest,
       ShouldPinBocaSWAWhenLockedOnSessionStartAndBundleUpdated) {
  const SessionID kWindowId = SessionID::NewUnique();
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
  EXPECT_CALL(*extensions_manager_ptr_, DisableExtensions)
      .Times(1)
      .InSequence(s);
  EXPECT_CALL(*system_web_app_manager_ptr_,
              SetPinStateForSystemWebAppWindow(true, kWindowId))
      .Times(1)
      .InSequence(s);

  ::boca::Bundle bundle;
  bundle.set_locked(true);
  session_manager_->OnSessionStarted("test_session_id", ::boca::UserIdentity());
  session_manager_->OnBundleUpdated(bundle);

  // Verify notification is shown.
  EXPECT_TRUE(fake_notifications_delegate_ptr_->WasNotificationShown(
      kOnTaskEnterLockedModeNotificationId));
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

  // Verify relevant notification is shown.
  EXPECT_TRUE(fake_notifications_delegate_ptr_->WasNotificationShown(
      kOnTaskBundleContentAddedNotificationId));
}

TEST_F(OnTaskSessionManagerTest, ShouldRemoveTabsWhenFewerTabsFoundInBundle) {
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
  EXPECT_CALL(*system_web_app_manager_ptr_,
              RemoveTabsWithTabIds(kWindowId, std::set<SessionID>{kTabId_2}))
      .Times(1);

  ::boca::Bundle bundle_1;
  bundle_1.add_content_configs()->set_url(kTestUrl1);
  bundle_1.add_content_configs()->set_url(kTestUrl2);
  session_manager_->OnBundleUpdated(bundle_1);

  // Verify notification is shown for newly added tabs.
  EXPECT_TRUE(fake_notifications_delegate_ptr_->WasNotificationShown(
      kOnTaskBundleContentAddedNotificationId));

  ::boca::Bundle bundle_2;
  bundle_2.add_content_configs()->set_url(kTestUrl1);
  session_manager_->OnBundleUpdated(bundle_2);

  // Verify notification is shown for removed content.
  EXPECT_TRUE(fake_notifications_delegate_ptr_->WasNotificationShown(
      kOnTaskBundleContentRemovedNotificationId));
}

TEST_F(OnTaskSessionManagerTest,
       NoNotificationShownWhenNoNewContentAddedOrRemoved) {
  // Inject tab id and nav restriction for testing purposes.
  const SessionID kTabId = SessionID::NewUnique();
  const ::boca::LockedNavigationOptions::NavigationType kNavRestriction =
      ::boca::LockedNavigationOptions::BLOCK_NAVIGATION;
  (*provider_url_tab_ids_map())[GURL(kTestUrl1)].insert(kTabId);
  (*provider_url_restriction_level_map())[GURL(kTestUrl1)] = kNavRestriction;

  // Attempt to trigger bundle update with same content.
  const SessionID kWindowId = SessionID::NewUnique();
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillRepeatedly(Return(kWindowId));
  EXPECT_CALL(*system_web_app_manager_ptr_, CreateBackgroundTabWithUrl(_, _, _))
      .Times(0);

  ::boca::Bundle bundle;
  ::boca::ContentConfig* const content_config =
      bundle.mutable_content_configs()->Add();
  content_config->set_url(kTestUrl1);
  content_config->mutable_locked_navigation_options()->set_navigation_type(
      kNavRestriction);
  session_manager_->OnBundleUpdated(bundle);

  // Verify no notification is shown because no new content was added or
  // removed.
  EXPECT_FALSE(fake_notifications_delegate_ptr_->WasNotificationShown(
      kOnTaskBundleContentAddedNotificationId));
  EXPECT_FALSE(fake_notifications_delegate_ptr_->WasNotificationShown(
      kOnTaskBundleContentRemovedNotificationId));
}

TEST_F(OnTaskSessionManagerTest,
       ShouldLaunchSWAWithNewBundleContentIfNoWindowFound) {
  const SessionID kWindowId = SessionID::NewUnique();
  const SessionID kTabId_1 = SessionID::NewUnique();
  const SessionID kTabId_2 = SessionID::NewUnique();

  // SWA launch should happen before we create tabs from the bundle, but the
  // order of tab creation does not matter.
  Sequence s1, s2;
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillOnce(
          Return(SessionID::InvalidValue()))  // No window found initially.
      .WillRepeatedly(Return(kWindowId));
  EXPECT_CALL(*system_web_app_manager_ptr_, LaunchSystemWebAppAsync(_))
      .InSequence(s1, s2)
      .WillOnce([](base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(kWindowId, GURL(kTestUrl1), _))
      .InSequence(s1)
      .WillOnce(Return(kTabId_1));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(kWindowId, GURL(kTestUrl2), _))
      .InSequence(s2)
      .WillOnce(Return(kTabId_2));

  ::boca::Bundle bundle;
  bundle.add_content_configs()->set_url(kTestUrl1);
  bundle.add_content_configs()->set_url(kTestUrl2);
  session_manager_->OnBundleUpdated(bundle);
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

  // Verify notification is shown.
  EXPECT_TRUE(fake_notifications_delegate_ptr_->WasNotificationShown(
      kOnTaskEnterLockedModeNotificationId));
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

TEST_F(OnTaskSessionManagerTest, ShouldUpdateRestrictionsToTabOnBundleUpdated) {
  const SessionID kWindowId = SessionID::NewUnique();
  const SessionID kTabId_1 = SessionID::NewUnique();
  Sequence s;
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillRepeatedly(Return(kWindowId));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(
                  kWindowId, GURL(kTestUrl1),
                  ::boca::LockedNavigationOptions::OPEN_NAVIGATION))
      .WillOnce(Return(kTabId_1));
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveTabID())
      .InSequence(s)
      .WillRepeatedly(Return(kTabId_1));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              RemoveTabsWithTabIds(kWindowId, std::set<SessionID>{kTabId_1}))
      .Times(1)
      .InSequence(s);
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(
                  kWindowId, GURL(kTestUrl1),
                  ::boca::LockedNavigationOptions::BLOCK_NAVIGATION))
      .InSequence(s)
      .WillOnce(Return(kTabId_1));
  ::boca::Bundle bundle;
  ::boca::ContentConfig* const content_config_1 =
      bundle.mutable_content_configs()->Add();
  content_config_1->set_url(kTestUrl1);
  content_config_1->mutable_locked_navigation_options()->set_navigation_type(
      ::boca::LockedNavigationOptions::OPEN_NAVIGATION);
  session_manager_->OnBundleUpdated(bundle);

  ::boca::Bundle bundle_2;
  ::boca::ContentConfig* const content_config_2 =
      bundle_2.mutable_content_configs()->Add();
  content_config_2->set_url(kTestUrl1);
  content_config_2->mutable_locked_navigation_options()->set_navigation_type(
      ::boca::LockedNavigationOptions::BLOCK_NAVIGATION);
  session_manager_->OnBundleUpdated(bundle_2);
}

TEST_F(OnTaskSessionManagerTest, OnAppReloadWithNoActiveWindow) {
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillOnce(Return(SessionID::InvalidValue()));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              PrepareSystemWebAppWindowForOnTask(_))
      .Times(0);
  session_manager_->OnAppReloaded();
}

TEST_F(OnTaskSessionManagerTest, RestoreTabsOnAppReload) {
  // Inject tab ids and nav restrictions tracked by the previous session for
  // testing purposes. It should fall back to
  // `::boca::LockedNavigationOptions::DOMAIN_NAVIGATION` if
  // there is no nav restriction being tracked.
  const SessionID kOldTabId1 = SessionID::NewUnique();
  const SessionID kOldTabId2 = SessionID::NewUnique();
  (*provider_url_tab_ids_map())[GURL(kTestUrl1)].insert(kOldTabId1);
  (*provider_url_restriction_level_map())[GURL(kTestUrl1)] =
      ::boca::LockedNavigationOptions::BLOCK_NAVIGATION;
  (*provider_url_tab_ids_map())[GURL(kTestUrl2)].insert(kOldTabId2);
  *active_session_id() = "test_session";

  // Attempt an app reload and verify tabs are restored with newer tab ids.
  const SessionID kWindowId = SessionID::NewUnique();
  const SessionID kTabId1 = SessionID::NewUnique();
  const SessionID kTabId2 = SessionID::NewUnique();
  const std::vector<boca::BocaWindowObserver*> kWindowObservers = {
      session_manager_->active_tab_tracker(), session_manager_.get()};
  Sequence s;
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillRepeatedly(Return(kWindowId));
  EXPECT_CALL(
      *system_web_app_manager_ptr_,
      SetWindowTrackerForSystemWebAppWindow(kWindowId, kWindowObservers))
      .Times(AtLeast(1));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              PrepareSystemWebAppWindowForOnTask(kWindowId))
      .Times(1)
      .InSequence(s);
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(
                  kWindowId, GURL(kTestUrl1),
                  ::boca::LockedNavigationOptions::BLOCK_NAVIGATION))
      .InSequence(s)
      .WillOnce(Return(kTabId1));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(
                  kWindowId, GURL(kTestUrl2),
                  ::boca::LockedNavigationOptions::DOMAIN_NAVIGATION))
      .InSequence(s)
      .WillOnce(Return(kTabId2));
  EXPECT_CALL(*system_web_app_manager_ptr_, SetPinStateForSystemWebAppWindow(
                                                /*pinned=*/false, kWindowId))
      .Times(1)
      .InSequence(s);
  session_manager_->OnAppReloaded();
  ASSERT_TRUE(
      testing::Mock::VerifyAndClearExpectations(system_web_app_manager_ptr_));
  EXPECT_THAT((*provider_url_tab_ids_map())[GURL(kTestUrl1)],
              ElementsAre(kTabId1));
  EXPECT_EQ((*provider_url_restriction_level_map())[GURL(kTestUrl1)],
            ::boca::LockedNavigationOptions::BLOCK_NAVIGATION);
  EXPECT_THAT((*provider_url_tab_ids_map())[GURL(kTestUrl2)],
              ElementsAre(kTabId2));
  EXPECT_EQ((*provider_url_restriction_level_map())[GURL(kTestUrl2)],
            ::boca::LockedNavigationOptions::DOMAIN_NAVIGATION);
}

TEST_F(OnTaskSessionManagerTest, LockWindowOnAppReload) {
  // Set window lock state for testing purposes.
  *should_lock_window() = true;
  *active_session_id() = "test_session";

  // Attempt an app reload and verify that the app window is locked.
  const SessionID kWindowId = SessionID::NewUnique();
  Sequence s;
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillRepeatedly(Return(kWindowId));
  const std::vector<boca::BocaWindowObserver*> kWindowObservers = {
      session_manager_->active_tab_tracker(), session_manager_.get()};
  EXPECT_CALL(
      *system_web_app_manager_ptr_,
      SetWindowTrackerForSystemWebAppWindow(kWindowId, kWindowObservers))
      .Times(AtLeast(1));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              PrepareSystemWebAppWindowForOnTask(kWindowId))
      .Times(1)
      .InSequence(s);
  EXPECT_CALL(*system_web_app_manager_ptr_, SetPinStateForSystemWebAppWindow(
                                                /*pinned=*/true, kWindowId))
      .Times(1)
      .InSequence(s);
  session_manager_->OnAppReloaded();
}

TEST_F(OnTaskSessionManagerTest,
       ShouldAddToProviderUrlTabIdsMapWhenTabIsAdded) {
  const SessionID kWindowId = SessionID::NewUnique();
  const SessionID kTabId_1 = SessionID::NewUnique();
  const SessionID kTabId_2 = SessionID::NewUnique();
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillOnce(Return(SessionID::InvalidValue()))  // Session init check.
      .WillRepeatedly(Return(kWindowId));
  EXPECT_CALL(*system_web_app_manager_ptr_, LaunchSystemWebAppAsync(_))
      .WillOnce([](base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(kWindowId, GURL(kTestUrl1), _))
      .WillOnce(Return(kTabId_1));

  // Init session.
  session_manager_->OnSessionStarted("test_session_id", ::boca::UserIdentity());

  // Update bundle.
  ::boca::Bundle bundle_1;
  bundle_1.add_content_configs()->set_url(kTestUrl1);
  session_manager_->OnBundleUpdated(bundle_1);

  // Simulate tab addition.
  const SessionID active_tab_id = kTabId_1;
  const SessionID tab_id = kTabId_2;
  session_manager_->OnTabAdded(active_tab_id, tab_id, GURL(kTestUrl1));
  EXPECT_THAT((*provider_url_tab_ids_map())[GURL(kTestUrl1)],
              UnorderedElementsAreArray({active_tab_id, tab_id}));
}

TEST_F(OnTaskSessionManagerTest, ShouldRemoveTabsAddedOutsideAnActiveSession) {
  const SessionID kWindowId = SessionID::NewUnique();
  const SessionID kActiveTabId = SessionID::NewUnique();
  const SessionID kTabId = SessionID::NewUnique();
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillRepeatedly(Return(kWindowId));
  bool tab_removed = false;
  EXPECT_CALL(*system_web_app_manager_ptr_,
              RemoveTabsWithTabIds(kWindowId, std::set<SessionID>{kTabId}))
      .WillOnce([&]() { tab_removed = true; });
  session_manager_->OnTabAdded(kActiveTabId, kTabId, GURL(kTestUrl1));
  ASSERT_TRUE(base::test::RunUntil([&]() { return tab_removed; }));
  EXPECT_FALSE((*provider_url_tab_ids_map()).contains(GURL(kTestUrl1)));
}

TEST_F(OnTaskSessionManagerTest,
       ShouldRemoveFromProviderUrlTabIdsMapWhenTabIsRemoved) {
  const SessionID kWindowId = SessionID::NewUnique();
  const SessionID kTabId_1 = SessionID::NewUnique();
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillRepeatedly(Return(kWindowId));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(kWindowId, GURL(kTestUrl1), _))
      .WillOnce(Return(kTabId_1));

  ::boca::Bundle bundle_1;
  bundle_1.add_content_configs()->set_url(kTestUrl1);
  session_manager_->OnBundleUpdated(bundle_1);
  const SessionID tab_id = kTabId_1;
  session_manager_->OnTabRemoved(tab_id);
  EXPECT_THAT((*provider_url_tab_ids_map())[GURL(kTestUrl1)], IsEmpty());
}

TEST_F(OnTaskSessionManagerTest,
       FocusBackToPreviousActiveTabOnUpdateRestrictionsForBundleUpdated) {
  const SessionID kWindowId = SessionID::NewUnique();
  const SessionID kTabId_1 = SessionID::NewUnique();
  const SessionID kTabId_2 = SessionID::NewUnique();
  Sequence s1, s2;
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillRepeatedly(Return(kWindowId));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(
                  kWindowId, GURL(kTestUrl1),
                  ::boca::LockedNavigationOptions::OPEN_NAVIGATION))
      .InSequence(s1)
      .WillOnce(Return(kTabId_1));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(
                  kWindowId, GURL(kTestUrl2),
                  ::boca::LockedNavigationOptions::OPEN_NAVIGATION))
      .InSequence(s1)
      .WillOnce(Return(kTabId_2));

  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveTabID())
      .InSequence(s2)
      .WillRepeatedly(Return(kTabId_1));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              RemoveTabsWithTabIds(kWindowId, std::set<SessionID>{kTabId_1}))
      .Times(1)
      .InSequence(s2);
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(
                  kWindowId, GURL(kTestUrl1),
                  ::boca::LockedNavigationOptions::BLOCK_NAVIGATION))
      .InSequence(s2)
      .WillOnce(Return(kTabId_1));
  EXPECT_CALL(*system_web_app_manager_ptr_, SwitchToTab(kTabId_1))
      .Times(1)
      .InSequence(s2);

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
      ::boca::LockedNavigationOptions::OPEN_NAVIGATION);
  session_manager_->OnBundleUpdated(bundle);

  // Update restrictions.
  ::boca::Bundle bundle_2;
  ::boca::ContentConfig* const content_config_3 =
      bundle_2.mutable_content_configs()->Add();
  content_config_3->set_url(kTestUrl1);
  content_config_3->mutable_locked_navigation_options()->set_navigation_type(
      ::boca::LockedNavigationOptions::BLOCK_NAVIGATION);
  ::boca::ContentConfig* const content_config_4 =
      bundle_2.mutable_content_configs()->Add();
  content_config_4->set_url(kTestUrl2);
  content_config_4->mutable_locked_navigation_options()->set_navigation_type(
      ::boca::LockedNavigationOptions::OPEN_NAVIGATION);
  session_manager_->OnBundleUpdated(bundle_2);
}

}  // namespace ash::boca
