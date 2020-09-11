// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/media_notification_provider_impl.h"

#include "ash/public/cpp/media_notification_provider_observer.h"
#include "base/unguessable_token.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service_factory.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_list_view.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"

using media_session::mojom::AudioFocusRequestState;
using media_session::mojom::AudioFocusRequestStatePtr;
using media_session::mojom::MediaSessionInfo;
using media_session::mojom::MediaSessionInfoPtr;

namespace {

class MockMediaNotificationProviderObserver
    : public ash::MediaNotificationProviderObserver {
 public:
  MOCK_METHOD0(OnNotificationListChanged, void());
  MOCK_METHOD0(OnNotificationListViewSizeChanged, void());
};

std::unique_ptr<KeyedService> CreateMediaNotificationService(
    content::BrowserContext* context) {
  return std::make_unique<MediaNotificationService>(
      Profile::FromBrowserContext(context), true /* show_from_all_profiles */);
}

}  // namespace

class MediaNotificationProviderImplTest : public testing::Test {
 protected:
  MediaNotificationProviderImplTest() {}
  ~MediaNotificationProviderImplTest() override {}

  void SetUp() override {
    testing::Test::SetUp();
    user_manager_->Initialize();
    CHECK(testing_profile_manager_.SetUp());

    AccountId account_id(AccountId::FromUserEmail("foo@test.com"));
    user_manager::User* user = user_manager_->AddPublicAccountUser(account_id);

    Profile* profile =
        testing_profile_manager_.CreateTestingProfile("test-profile");
    chromeos::ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                                      profile);
    chromeos::ProfileHelper::Get()->SetProfileToUserMappingForTesting(user);

    user_manager_->LoginUser(account_id);
    DCHECK(user_manager_->GetPrimaryUser());

    MediaNotificationServiceFactory::GetInstance()->SetTestingFactory(
        profile, base::BindRepeating(&CreateMediaNotificationService));

    provider_ = std::make_unique<MediaNotificationProviderImpl>();
    mock_observer_ = std::make_unique<MockMediaNotificationProviderObserver>();

    session_manager_.NotifyUserProfileLoaded(account_id);
    DCHECK(provider_->service_for_testing());

    provider_->AddObserver(mock_observer_.get());
  }

  void TearDown() override {
    mock_observer_.reset();
    provider_.reset();
    user_manager_->Destroy();
    testing::Test::TearDown();
  }

  void SimulateShowNotification(base::UnguessableToken id) {
    MediaSessionInfoPtr session_info(MediaSessionInfo::New());
    session_info->is_controllable = true;

    AudioFocusRequestStatePtr focus(AudioFocusRequestState::New());
    focus->request_id = id;
    focus->session_info = std::move(session_info);

    provider_->service_for_testing()->OnFocusGained(std::move(focus));
    provider_->service_for_testing()->ShowNotification(id.ToString());
  }

  void SimulateHideNotification(base::UnguessableToken id) {
    provider_->service_for_testing()->HideNotification(id.ToString());
  }

  MockMediaNotificationProviderObserver* observer() {
    return mock_observer_.get();
  }

  MediaNotificationProviderImpl* provider() { return provider_.get(); }

  content::BrowserTaskEnvironment browser_environment;

 private:
  session_manager::SessionManager session_manager_;
  chromeos::FakeChromeUserManager* user_manager_{
      new chromeos::FakeChromeUserManager()};
  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};
  views::LayoutProvider layout_provider;

  std::unique_ptr<MockMediaNotificationProviderObserver> mock_observer_;
  std::unique_ptr<MediaNotificationProviderImpl> provider_;
};

TEST_F(MediaNotificationProviderImplTest, NotificationListTest) {
  auto id_1 = base::UnguessableToken::Create();
  auto id_2 = base::UnguessableToken::Create();

  EXPECT_CALL(*observer(), OnNotificationListViewSizeChanged).Times(2);
  SimulateShowNotification(id_1);
  SimulateShowNotification(id_2);
  std::unique_ptr<views::View> view =
      provider()->GetMediaNotificationListView(SK_ColorBLACK, 1);

  auto* notification_list_view =
      static_cast<MediaNotificationListView*>(view.get());
  EXPECT_EQ(notification_list_view->notifications_for_testing().size(), 2u);

  EXPECT_CALL(*observer(), OnNotificationListViewSizeChanged);
  SimulateHideNotification(id_1);
  EXPECT_EQ(notification_list_view->notifications_for_testing().size(), 1u);

  provider()->OnBubbleClosing();
}

TEST_F(MediaNotificationProviderImplTest, NotifyObserverOnListChangeTest) {
  auto id = base::UnguessableToken::Create();

  // Expecting 2 calls: one when MediaSessionNotificationItem is created in
  // MediaNotificationService::OnFocusgained, one when
  // MediaNotificationService::ShowNotification is called in
  // SimulateShownotification.
  EXPECT_CALL(*observer(), OnNotificationListChanged).Times(2);
  SimulateShowNotification(id);

  EXPECT_CALL(*observer(), OnNotificationListChanged);
  SimulateHideNotification(id);
}
