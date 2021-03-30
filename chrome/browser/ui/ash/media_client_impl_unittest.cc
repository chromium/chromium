// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/media_client_impl.h"

#include <memory>

#include "ash/public/cpp/media_controller.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/extensions/media_player_api.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/account_id/account_id.h"
#include "ui/base/accelerators/media_keys_listener.h"

namespace {

class TestMediaController : public ash::MediaController {
 public:
  TestMediaController() = default;
  ~TestMediaController() override = default;

  // ash::MediaController:
  void SetClient(ash::MediaClient* client) override {}
  void SetForceMediaClientKeyHandling(bool enabled) override {
    force_media_client_key_handling_ = enabled;
  }
  void NotifyCaptureState(
      const base::flat_map<AccountId, ash::MediaCaptureState>& capture_states)
      override {}

  void NotifyVmMediaNotificationState(bool camera,
                                      bool mic,
                                      bool camera_and_mic) override {}

  bool force_media_client_key_handling() const {
    return force_media_client_key_handling_;
  }

 private:
  bool force_media_client_key_handling_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestMediaController);
};

class TestMediaKeysDelegate : public ui::MediaKeysListener::Delegate {
 public:
  TestMediaKeysDelegate() = default;
  ~TestMediaKeysDelegate() override = default;

  void OnMediaKeysAccelerator(const ui::Accelerator& accelerator) override {
    last_media_key_ = accelerator;
  }

  base::Optional<ui::Accelerator> ConsumeLastMediaKey() {
    base::Optional<ui::Accelerator> key = last_media_key_;
    last_media_key_.reset();
    return key;
  }

 private:
  base::Optional<ui::Accelerator> last_media_key_;

  DISALLOW_COPY_AND_ASSIGN(TestMediaKeysDelegate);
};

}  // namespace

class MediaClientTest : public BrowserWithTestWindowTest {
 public:
  MediaClientTest() = default;
  ~MediaClientTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    alt_window_ = CreateBrowserWindow();
    alt_browser_ = CreateBrowser(alt_profile(), Browser::TYPE_NORMAL, false,
                                 alt_window_.get());

    extensions::MediaPlayerAPI::Get(profile());

    test_delegate_ = std::make_unique<TestMediaKeysDelegate>();

    media_controller_resetter_ =
        std::make_unique<ash::MediaController::ScopedResetterForTest>();
    test_media_controller_ = std::make_unique<TestMediaController>();

    media_client_ = std::make_unique<MediaClientImpl>();
    media_client_->InitForTesting(test_media_controller_.get());

    BrowserList::SetLastActive(browser());

    ASSERT_FALSE(test_media_controller_->force_media_client_key_handling());
    ASSERT_EQ(base::nullopt, delegate()->ConsumeLastMediaKey());
  }

  void TearDown() override {
    media_client_.reset();
    test_media_controller_.reset();
    media_controller_resetter_.reset();
    test_delegate_.reset();

    alt_browser_->tab_strip_model()->CloseAllTabs();
    alt_browser_.reset();
    alt_window_.reset();

    BrowserWithTestWindowTest::TearDown();
  }

  MediaClientImpl* client() { return media_client_.get(); }

  TestMediaController* controller() { return test_media_controller_.get(); }

  Profile* alt_profile() { return profile()->GetPrimaryOTRProfile(); }

  Browser* alt_browser() { return alt_browser_.get(); }

  TestMediaKeysDelegate* delegate() { return test_delegate_.get(); }

 private:
  std::unique_ptr<TestMediaKeysDelegate> test_delegate_;
  std::unique_ptr<ash::MediaController::ScopedResetterForTest>
      media_controller_resetter_;
  std::unique_ptr<TestMediaController> test_media_controller_;
  std::unique_ptr<MediaClientImpl> media_client_;

  std::unique_ptr<Browser> alt_browser_;
  std::unique_ptr<BrowserWindow> alt_window_;

  DISALLOW_COPY_AND_ASSIGN(MediaClientTest);
};

TEST_F(MediaClientTest, HandleMediaAccelerators) {
  const struct {
    ui::Accelerator accelerator;
    base::RepeatingClosure client_handler;
  } kTestCases[] = {
      {ui::Accelerator(ui::VKEY_MEDIA_PLAY_PAUSE, ui::EF_NONE),
       base::BindRepeating(&MediaClientImpl::HandleMediaPlayPause,
                           base::Unretained(client()))},
      {ui::Accelerator(ui::VKEY_MEDIA_PLAY, ui::EF_NONE),
       base::BindRepeating(&MediaClientImpl::HandleMediaPlay,
                           base::Unretained(client()))},
      {ui::Accelerator(ui::VKEY_MEDIA_PAUSE, ui::EF_NONE),
       base::BindRepeating(&MediaClientImpl::HandleMediaPause,
                           base::Unretained(client()))},
      {ui::Accelerator(ui::VKEY_MEDIA_STOP, ui::EF_NONE),
       base::BindRepeating(&MediaClientImpl::HandleMediaStop,
                           base::Unretained(client()))},
      {ui::Accelerator(ui::VKEY_MEDIA_NEXT_TRACK, ui::EF_NONE),
       base::BindRepeating(&MediaClientImpl::HandleMediaNextTrack,
                           base::Unretained(client()))},
      {ui::Accelerator(ui::VKEY_MEDIA_PREV_TRACK, ui::EF_NONE),
       base::BindRepeating(&MediaClientImpl::HandleMediaPrevTrack,
                           base::Unretained(client()))},
      {ui::Accelerator(ui::VKEY_OEM_103, ui::EF_NONE),
       base::BindRepeating(&MediaClientImpl::HandleMediaSeekBackward,
                           base::Unretained(client()))},
      {ui::Accelerator(ui::VKEY_OEM_104, ui::EF_NONE),
       base::BindRepeating(&MediaClientImpl::HandleMediaSeekForward,
                           base::Unretained(client()))}};

  for (auto& test : kTestCases) {
    SCOPED_TRACE(::testing::Message()
                 << "accelerator key:" << test.accelerator.key_code());

    // Enable custom media key handling for the current browser. Ensure that the
    // client set the override on the controller.
    client()->EnableCustomMediaKeyHandler(profile(), delegate());
    EXPECT_TRUE(controller()->force_media_client_key_handling());

    // Simulate the media key and check that the delegate received it.
    test.client_handler.Run();
    EXPECT_EQ(test.accelerator, delegate()->ConsumeLastMediaKey());

    // Change the active browser and ensure the override was disabled.
    BrowserList::SetLastActive(alt_browser());
    EXPECT_FALSE(controller()->force_media_client_key_handling());

    // Simulate the media key and check that the delegate did not receive it.
    test.client_handler.Run();
    EXPECT_EQ(base::nullopt, delegate()->ConsumeLastMediaKey());

    // Change the active browser back and ensure the override was enabled.
    BrowserList::SetLastActive(browser());
    EXPECT_TRUE(controller()->force_media_client_key_handling());

    // Simulate the media key and check the delegate received it.
    test.client_handler.Run();
    EXPECT_EQ(test.accelerator, delegate()->ConsumeLastMediaKey());

    // Disable custom media key handling for the current browser and ensure the
    // override was disabled.
    client()->DisableCustomMediaKeyHandler(profile(), delegate());
    EXPECT_FALSE(controller()->force_media_client_key_handling());

    // Simulate the media key and check the delegate did not receive it.
    test.client_handler.Run();
    EXPECT_EQ(base::nullopt, delegate()->ConsumeLastMediaKey());
  }
}
