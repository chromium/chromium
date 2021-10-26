// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/media_notification_provider_impl.h"

#include "ash/public/cpp/media_notification_provider_observer.h"
#include "base/unguessable_token.h"
#include "components/global_media_controls/public/media_session_item_producer.h"
#include "components/global_media_controls/public/views/media_item_ui_list_view.h"
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

}  // namespace

class MediaNotificationProviderImplTest : public testing::Test {
 protected:
  MediaNotificationProviderImplTest() {}
  ~MediaNotificationProviderImplTest() override {}

  void SetUp() override {
    testing::Test::SetUp();

    provider_ = std::make_unique<MediaNotificationProviderImpl>();
    mock_observer_ = std::make_unique<MockMediaNotificationProviderObserver>();
    provider_->AddObserver(mock_observer_.get());
  }

  void TearDown() override {
    mock_observer_.reset();
    provider_.reset();
    testing::Test::TearDown();
  }

  void SimulateShowNotification(base::UnguessableToken id) {
    MediaSessionInfoPtr session_info(MediaSessionInfo::New());
    session_info->is_controllable = true;

    AudioFocusRequestStatePtr focus(AudioFocusRequestState::New());
    focus->request_id = id;
    focus->session_info = std::move(session_info);

    provider_->media_session_item_producer_for_testing()->OnFocusGained(
        std::move(focus));
    provider_->media_session_item_producer_for_testing()->ActivateItem(
        id.ToString());
  }

  void SimulateHideNotification(base::UnguessableToken id) {
    provider_->media_session_item_producer_for_testing()->HideItem(
        id.ToString());
  }

  MockMediaNotificationProviderObserver* observer() {
    return mock_observer_.get();
  }

  MediaNotificationProviderImpl* provider() { return provider_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  views::LayoutProvider layout_provider_;

  std::unique_ptr<MockMediaNotificationProviderObserver> mock_observer_;

  std::unique_ptr<MediaNotificationProviderImpl> provider_;
};

TEST_F(MediaNotificationProviderImplTest, NotificationListTest) {
  auto id_1 = base::UnguessableToken::Create();
  auto id_2 = base::UnguessableToken::Create();

  EXPECT_CALL(*observer(), OnNotificationListViewSizeChanged).Times(2);
  SimulateShowNotification(id_1);
  SimulateShowNotification(id_2);
  provider()->SetColorTheme(media_message_center::NotificationTheme());
  std::unique_ptr<views::View> view =
      provider()->GetMediaNotificationListView(1);

  auto* notification_list_view =
      static_cast<global_media_controls::MediaItemUIListView*>(view.get());
  EXPECT_EQ(notification_list_view->items_for_testing().size(), 2u);

  EXPECT_CALL(*observer(), OnNotificationListViewSizeChanged);
  SimulateHideNotification(id_1);
  EXPECT_EQ(notification_list_view->items_for_testing().size(), 1u);

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
