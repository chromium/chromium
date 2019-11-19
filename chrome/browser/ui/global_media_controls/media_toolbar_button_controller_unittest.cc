// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ui/global_media_controls/media_dialog_delegate.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller_delegate.h"
#include "chrome/browser/ui/global_media_controls/overlay_media_notification.h"
#include "chrome/test/base/testing_profile.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_message_center/media_notification_util.h"
#include "components/media_message_center/media_session_notification_item.h"
#include "content/public/test/browser_task_environment.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using media_session::mojom::AudioFocusRequestState;
using media_session::mojom::AudioFocusRequestStatePtr;
using media_session::mojom::MediaSessionInfo;
using media_session::mojom::MediaSessionInfoPtr;
using testing::_;

namespace {

class MockMediaToolbarButtonControllerDelegate
    : public MediaToolbarButtonControllerDelegate {
 public:
  MockMediaToolbarButtonControllerDelegate() = default;
  ~MockMediaToolbarButtonControllerDelegate() override = default;

  // MediaToolbarButtonControllerDelegate implementation.
  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());
  MOCK_METHOD0(Enable, void());
  MOCK_METHOD0(Disable, void());
};

class MockMediaDialogDelegate : public MediaDialogDelegate {
 public:
  MockMediaDialogDelegate() = default;
  ~MockMediaDialogDelegate() override { Close(); }

  void Open(MediaNotificationService* service) {
    ASSERT_NE(nullptr, service);
    service_ = service;
    service_->SetDialogDelegate(this);
  }

  void Close() {
    if (!service_)
      return;

    service_->SetDialogDelegate(nullptr);
    service_ = nullptr;
  }

  // MediaDialogDelegate implementation.
  MOCK_METHOD2(
      ShowMediaSession,
      MediaNotificationContainerImpl*(
          const std::string& id,
          base::WeakPtr<media_message_center::MediaNotificationItem> item));
  MOCK_METHOD1(HideMediaSession, void(const std::string& id));
  MOCK_METHOD2(PopOut,
               std::unique_ptr<OverlayMediaNotification>(const std::string& id,
                                                         gfx::Rect bounds));

 private:
  MediaNotificationService* service_;

  DISALLOW_COPY_AND_ASSIGN(MockMediaDialogDelegate);
};

}  // anonymous namespace

class MediaToolbarButtonControllerTest : public testing::Test {
 public:
  MediaToolbarButtonControllerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME,
                          base::test::TaskEnvironment::MainThreadType::UI),
        service_(&profile_, nullptr) {}
  ~MediaToolbarButtonControllerTest() override = default;

  void SetUp() override {
    controller_ =
        std::make_unique<MediaToolbarButtonController>(&delegate_, &service_);
  }

  void TearDown() override { controller_.reset(); }

 protected:
  void AdvanceClockMilliseconds(int milliseconds) {
    task_environment_.FastForwardBy(
        base::TimeDelta::FromMilliseconds(milliseconds));
  }

  base::UnguessableToken SimulatePlayingControllableMedia() {
    base::UnguessableToken id = base::UnguessableToken::Create();
    SimulateFocusGained(id, true);
    SimulateNecessaryMetadata(id);
    return id;
  }

  AudioFocusRequestStatePtr CreateFocusRequest(const base::UnguessableToken& id,
                                               bool controllable) {
    MediaSessionInfoPtr session_info(MediaSessionInfo::New());
    session_info->is_controllable = controllable;

    AudioFocusRequestStatePtr focus(AudioFocusRequestState::New());
    focus->request_id = id;
    focus->session_info = std::move(session_info);
    return focus;
  }

  void SimulateFocusGained(const base::UnguessableToken& id,
                           bool controllable) {
    service_.OnFocusGained(CreateFocusRequest(id, controllable));
  }

  void SimulateFocusLost(const base::UnguessableToken& id) {
    AudioFocusRequestStatePtr focus(AudioFocusRequestState::New());
    focus->request_id = id;
    service_.OnFocusLost(std::move(focus));
  }

  void SimulateNecessaryMetadata(const base::UnguessableToken& id) {
    // In order for the MediaNotificationItem to tell the
    // MediaNotificationService to show a media session, that session needs
    // a title and artist. Typically this would happen through the media session
    // service, but since the service doesn't run for this test, we'll manually
    // grab the MediaNotificationItem from the MediaNotificationService and
    // set the metadata.
    auto item_itr = service_.sessions_.find(id.ToString());
    ASSERT_NE(service_.sessions_.end(), item_itr);

    media_session::MediaMetadata metadata;
    metadata.title = base::ASCIIToUTF16("title");
    metadata.artist = base::ASCIIToUTF16("artist");
    item_itr->second.item()->MediaSessionMetadataChanged(std::move(metadata));
  }

  void SimulateDialogOpened(MockMediaDialogDelegate* delegate) {
    delegate->Open(&service_);
  }

  MockMediaToolbarButtonControllerDelegate& delegate() { return delegate_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  MockMediaToolbarButtonControllerDelegate delegate_;
  TestingProfile profile_;
  MediaNotificationService service_;
  std::unique_ptr<MediaToolbarButtonController> controller_;

  DISALLOW_COPY_AND_ASSIGN(MediaToolbarButtonControllerTest);
};

TEST_F(MediaToolbarButtonControllerTest, HidesAfterTimeoutAndShowsAgainOnPlay) {
  // First, show the button by playing media.
  EXPECT_CALL(delegate(), Show());
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // Then, stop playing media so the button is disabled, but hasn't been hidden
  // yet.
  EXPECT_CALL(delegate(), Disable());
  EXPECT_CALL(delegate(), Hide()).Times(0);
  SimulateFocusLost(id);
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // If the time hasn't elapsed yet, the button should still not be hidden.
  EXPECT_CALL(delegate(), Hide()).Times(0);
  AdvanceClockMilliseconds(2400);
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // Once the time is elapsed, the button should be hidden.
  EXPECT_CALL(delegate(), Hide());
  AdvanceClockMilliseconds(200);
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // If media starts playing again, we should show and enable the button.
  EXPECT_CALL(delegate(), Show());
  EXPECT_CALL(delegate(), Enable());
  SimulatePlayingControllableMedia();
  testing::Mock::VerifyAndClearExpectations(&delegate());
}

TEST_F(MediaToolbarButtonControllerTest, DoesNotDisableButtonIfDialogIsOpen) {
  // First, show the button.
  EXPECT_CALL(delegate(), Show());
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // Then, open a dialog.
  MockMediaDialogDelegate dialog_delegate;
  EXPECT_CALL(dialog_delegate, ShowMediaSession(id.ToString(), _));
  SimulateDialogOpened(&dialog_delegate);

  // Then, have the session lose focus. This should not disable the button when
  // a dialog is present (since the button can still be used to close the
  // dialog).
  EXPECT_CALL(delegate(), Disable()).Times(0);
  SimulateFocusLost(id);
  testing::Mock::VerifyAndClearExpectations(&delegate());
}

TEST_F(MediaToolbarButtonControllerTest,
       DoesNotHideIfMediaStartsPlayingWithinTimeout) {
  // First, show the button.
  EXPECT_CALL(delegate(), Show());
  base::UnguessableToken id = SimulatePlayingControllableMedia();
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // Then, stop playing media so the button is disabled, but hasn't been hidden
  // yet.
  EXPECT_CALL(delegate(), Disable());
  EXPECT_CALL(delegate(), Hide()).Times(0);
  SimulateFocusLost(id);
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // If the time hasn't elapsed yet, we should still not be hidden.
  EXPECT_CALL(delegate(), Hide()).Times(0);
  AdvanceClockMilliseconds(2400);
  testing::Mock::VerifyAndClearExpectations(&delegate());

  // If media starts playing again, we should show and enable the button.
  EXPECT_CALL(delegate(), Show());
  EXPECT_CALL(delegate(), Enable());
  SimulatePlayingControllableMedia();
  testing::Mock::VerifyAndClearExpectations(&delegate());
}
