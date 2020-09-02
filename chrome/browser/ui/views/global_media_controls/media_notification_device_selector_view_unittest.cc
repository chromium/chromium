// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_notification_device_selector_view.h"

#include "base/callback_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/util/ranges/algorithm.h"
#include "chrome/browser/ui/global_media_controls/media_notification_device_provider.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_device_selector_view_delegate.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "media/audio/audio_device_description.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/color_palette.h"

class MediaNotificationContainerObserver;

namespace {

class MockMediaNotificationDeviceProvider
    : public MediaNotificationDeviceProvider {
 public:
  MockMediaNotificationDeviceProvider() = default;
  ~MockMediaNotificationDeviceProvider() override = default;

  void AddDevice(const std::string& device_name, const std::string& device_id) {
    device_descriptions_.emplace_back(device_name, device_id, "");
  }

  void ResetDevices() { device_descriptions_.clear(); }

  void RunUICallback() { output_devices_callback_.Run(device_descriptions_); }

  std::unique_ptr<MediaNotificationDeviceProvider::
                      GetOutputDevicesCallbackList::Subscription>
  RegisterOutputDeviceDescriptionsCallback(
      GetOutputDevicesCallback cb) override {
    output_devices_callback_ = std::move(cb);
    RunUICallback();
    return std::unique_ptr<MockMediaNotificationDeviceProvider::
                               GetOutputDevicesCallbackList::Subscription>(
        nullptr);
  }

  MOCK_METHOD(void,
              GetOutputDeviceDescriptions,
              (media::AudioSystem::OnDeviceDescriptionsCallback),
              (override));

 private:
  media::AudioDeviceDescriptions device_descriptions_;

  GetOutputDevicesCallback output_devices_callback_;
};

class MockMediaNotificationDeviceSelectorViewDelegate
    : public MediaNotificationDeviceSelectorViewDelegate {
 public:
  MockMediaNotificationDeviceSelectorViewDelegate() {
    provider_ = std::make_unique<MockMediaNotificationDeviceProvider>();
  }

  MOCK_METHOD(void,
              OnAudioSinkChosen,
              (const std::string& sink_id),
              (override));
  MOCK_METHOD(void, OnDeviceSelectorViewSizeChanged, (), (override));

  std::unique_ptr<MediaNotificationDeviceProvider::
                      GetOutputDevicesCallbackList::Subscription>
  RegisterAudioOutputDeviceDescriptionsCallback(
      MediaNotificationDeviceProvider::GetOutputDevicesCallbackList::
          CallbackType callback) override {
    return provider_->RegisterOutputDeviceDescriptionsCallback(
        std::move(callback));
  }

  MockMediaNotificationDeviceProvider* GetProvider() { return provider_.get(); }

  std::unique_ptr<base::RepeatingCallbackList<void(bool)>::Subscription>
  RegisterIsAudioOutputDeviceSwitchingSupportedCallback(
      base::RepeatingCallback<void(bool)> callback) override {
    callback.Run(supports_switching);
    supports_switching_callback_ = std::move(callback);
    return nullptr;
  }

  void RunSupportsDeviceSwitchingCallback() {
    supports_switching_callback_.Run(supports_switching);
  }

  bool supports_switching = true;

 private:
  std::unique_ptr<MockMediaNotificationDeviceProvider> provider_;
  base::RepeatingCallback<void(bool)> supports_switching_callback_;
};

}  // anonymous namespace

class MediaNotificationDeviceSelectorViewTest : public ChromeViewsTestBase {
 public:
  MediaNotificationDeviceSelectorViewTest() = default;
  ~MediaNotificationDeviceSelectorViewTest() override = default;

  // ChromeViewsTestBase
  void SetUp() override { ChromeViewsTestBase::SetUp(); }

  void TearDown() override {
    view_.reset();
    ChromeViewsTestBase::TearDown();
  }

  void SimulateButtonClick(views::View* view) {
    view_->ButtonPressed(
        static_cast<views::Button*>(view),
        ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), 0, 0));
  }

  static std::string EntryLabelText(views::View* entry_view) {
    return MediaNotificationDeviceSelectorView::get_entry_label_for_testing(
        entry_view);
  }

  static bool IsHighlighted(views::View* entry_view) {
    return MediaNotificationDeviceSelectorView::
        get_entry_is_highlighted_for_testing(entry_view);
  }

  std::string GetButtonText(views::View* view) {
    return base::UTF16ToUTF8(static_cast<views::LabelButton*>(view)->GetText());
  }

  std::unique_ptr<MediaNotificationDeviceSelectorView> view_;
};

TEST_F(MediaNotificationDeviceSelectorViewTest, DeviceButtonsCreated) {
  // Buttons should be created for every device reported by the provider
  MockMediaNotificationDeviceSelectorViewDelegate delegate;
  auto* provider = delegate.GetProvider();
  provider->AddDevice("Speaker", "1");
  provider->AddDevice("Headphones", "2");
  provider->AddDevice("Earbuds", "3");

  view_ = std::make_unique<MediaNotificationDeviceSelectorView>(
      &delegate, "1", gfx::kPlaceholderColor, gfx::kPlaceholderColor);

  ASSERT_TRUE(view_->audio_device_entries_container_ != nullptr);

  auto container_children = view_->audio_device_entries_container_->children();
  ASSERT_EQ(container_children.size(), 3u);

  EXPECT_EQ(EntryLabelText(container_children.at(0)), "Speaker");
  EXPECT_EQ(EntryLabelText(container_children.at(1)), "Headphones");
  EXPECT_EQ(EntryLabelText(container_children.at(2)), "Earbuds");
}

TEST_F(MediaNotificationDeviceSelectorViewTest,
       ExpandButtonOpensEntryContainer) {
  MockMediaNotificationDeviceSelectorViewDelegate delegate;
  auto* provider = delegate.GetProvider();
  provider->AddDevice("Speaker", "1");
  provider->AddDevice("Headphones", "2");
  provider->AddDevice("Earbuds", "3");

  view_ = std::make_unique<MediaNotificationDeviceSelectorView>(
      &delegate, "1", gfx::kPlaceholderColor, gfx::kPlaceholderColor);

  ASSERT_TRUE(view_->expand_button_);
  EXPECT_FALSE(view_->audio_device_entries_container_->GetVisible());
  SimulateButtonClick(view_->get_expand_button_for_testing());
  EXPECT_TRUE(view_->audio_device_entries_container_->GetVisible());
}

TEST_F(MediaNotificationDeviceSelectorViewTest,
       DeviceButtonClickNotifiesContainer) {
  // When buttons are clicked the media notification delegate should be
  // informed.
  MockMediaNotificationDeviceSelectorViewDelegate delegate;
  auto* provider = delegate.GetProvider();
  provider->AddDevice("Speaker", "1");
  provider->AddDevice("Headphones", "2");
  provider->AddDevice("Earbuds", "3");

  view_ = std::make_unique<MediaNotificationDeviceSelectorView>(
      &delegate, "1", gfx::kPlaceholderColor, gfx::kPlaceholderColor);

  EXPECT_CALL(delegate, OnAudioSinkChosen("1")).Times(1);
  EXPECT_CALL(delegate, OnAudioSinkChosen("2")).Times(1);
  EXPECT_CALL(delegate, OnAudioSinkChosen("3")).Times(1);

  for (views::View* child :
       view_->audio_device_entries_container_->children()) {
    SimulateButtonClick(child);
  }
}

TEST_F(MediaNotificationDeviceSelectorViewTest, CurrentDeviceHighlighted) {
  // The 'current' audio device should be highlighted in the UI and appear
  // before other devices.
  MockMediaNotificationDeviceSelectorViewDelegate delegate;
  auto* provider = delegate.GetProvider();
  provider->AddDevice("Speaker", "1");
  provider->AddDevice("Headphones", "2");
  provider->AddDevice("Earbuds", "3");

  view_ = std::make_unique<MediaNotificationDeviceSelectorView>(
      &delegate, "3", gfx::kPlaceholderColor, gfx::kPlaceholderColor);

  auto* first_entry =
      view_->audio_device_entries_container_->children().front();
  EXPECT_EQ(EntryLabelText(first_entry), "Earbuds");
  EXPECT_TRUE(IsHighlighted(first_entry));
}

TEST_F(MediaNotificationDeviceSelectorViewTest, DeviceHighlightedOnChange) {
  // When the audio output device changes, the UI should highlight that one.
  MockMediaNotificationDeviceSelectorViewDelegate delegate;
  auto* provider = delegate.GetProvider();
  provider->AddDevice("Speaker", "1");
  provider->AddDevice("Headphones", "2");
  provider->AddDevice("Earbuds", "3");

  view_ = std::make_unique<MediaNotificationDeviceSelectorView>(
      &delegate, "1", gfx::kPlaceholderColor, gfx::kPlaceholderColor);

  auto& container_children = view_->audio_device_entries_container_->children();

  // There should be only one highlighted button. It should be the first button.
  // It's text should be "Speaker"
  EXPECT_EQ(util::ranges::count_if(container_children, IsHighlighted), 1);
  EXPECT_EQ(util::ranges::find_if(container_children, IsHighlighted),
            container_children.begin());
  EXPECT_EQ(EntryLabelText(container_children.front()), "Speaker");

  // Simulate a device change
  view_->UpdateCurrentAudioDevice("3");

  // The button for "Earbuds" should come before all others & be highlighted.
  EXPECT_EQ(util::ranges::count_if(container_children, IsHighlighted), 1);
  EXPECT_EQ(util::ranges::find_if(container_children, IsHighlighted),
            container_children.begin());
  EXPECT_EQ(EntryLabelText(container_children.front()), "Earbuds");
}

TEST_F(MediaNotificationDeviceSelectorViewTest, DeviceButtonsChange) {
  // If the device provider reports a change in connect audio devices, the UI
  // should update accordingly.
  MockMediaNotificationDeviceSelectorViewDelegate delegate;
  auto* provider = delegate.GetProvider();
  provider->AddDevice("Speaker", "1");
  provider->AddDevice("Headphones", "2");
  provider->AddDevice("Earbuds", "3");

  view_ = std::make_unique<MediaNotificationDeviceSelectorView>(
      &delegate, "1", gfx::kPlaceholderColor, gfx::kPlaceholderColor);

  provider->ResetDevices();
  // Make "Monitor" the default device.
  provider->AddDevice("Monitor",
                      media::AudioDeviceDescription::kDefaultDeviceId);
  provider->RunUICallback();

  {
    auto& container_children =
        view_->audio_device_entries_container_->children();
    EXPECT_EQ(container_children.size(), 1u);
    ASSERT_FALSE(container_children.empty());
    EXPECT_EQ(EntryLabelText(container_children.front()), "Monitor");

    // When the device highlighted in the UI is removed, the UI should fall back
    // to highlighting the default device.
    EXPECT_TRUE(IsHighlighted(container_children.front()));
  }

  provider->ResetDevices();
  provider->AddDevice("Speaker", "1");
  provider->AddDevice("Headphones", "2");
  provider->AddDevice("Earbuds", "3");
  provider->RunUICallback();

  {
    auto& container_children =
        view_->audio_device_entries_container_->children();
    EXPECT_EQ(container_children.size(), 3u);
    ASSERT_FALSE(container_children.empty());

    // When the device highlighted in the UI is removed, and there is no default
    // device, the UI should not highlight any of the devices.
    for (auto* device_view : container_children) {
      EXPECT_FALSE(IsHighlighted(device_view));
    }
  }
}

TEST_F(MediaNotificationDeviceSelectorViewTest, VisibilityChanges) {
  // The device selector view should become hidden when there is only one
  // unique device.
  MockMediaNotificationDeviceSelectorViewDelegate delegate;
  auto* provider = delegate.GetProvider();
  provider->AddDevice("Speaker", "1");
  provider->AddDevice(media::AudioDeviceDescription::GetDefaultDeviceName(),
                      media::AudioDeviceDescription::kDefaultDeviceId);

  EXPECT_CALL(delegate, OnDeviceSelectorViewSizeChanged).Times(2);
  view_ = std::make_unique<MediaNotificationDeviceSelectorView>(
      &delegate, media::AudioDeviceDescription::kDefaultDeviceId,
      gfx::kPlaceholderColor, gfx::kPlaceholderColor);
  EXPECT_FALSE(view_->GetVisible());

  testing::Mock::VerifyAndClearExpectations(&delegate);

  provider->ResetDevices();
  provider->AddDevice("Speaker", "1");
  provider->AddDevice("Headphones",
                      media::AudioDeviceDescription::kDefaultDeviceId);
  EXPECT_CALL(delegate, OnDeviceSelectorViewSizeChanged).Times(1);
  provider->RunUICallback();
  EXPECT_TRUE(view_->GetVisible());
  testing::Mock::VerifyAndClearExpectations(&delegate);

  provider->ResetDevices();
  provider->AddDevice("Speaker", "1");
  provider->AddDevice("Headphones", "2");
  provider->AddDevice(media::AudioDeviceDescription::GetDefaultDeviceName(),
                      media::AudioDeviceDescription::kDefaultDeviceId);
  EXPECT_CALL(delegate, OnDeviceSelectorViewSizeChanged).Times(1);
  provider->RunUICallback();
  EXPECT_TRUE(view_->GetVisible());
  testing::Mock::VerifyAndClearExpectations(&delegate);
}

TEST_F(MediaNotificationDeviceSelectorViewTest, DeviceChangeIsNotSupported) {
  MockMediaNotificationDeviceSelectorViewDelegate delegate;
  auto* provider = delegate.GetProvider();
  provider->AddDevice("Speaker", "1");
  provider->AddDevice("Headphones", "2");
  provider->AddDevice("Earbuds", "3");
  delegate.supports_switching = false;

  view_ = std::make_unique<MediaNotificationDeviceSelectorView>(
      &delegate, "1", gfx::kPlaceholderColor, gfx::kPlaceholderColor);
  EXPECT_FALSE(view_->GetVisible());

  delegate.supports_switching = true;
  delegate.RunSupportsDeviceSwitchingCallback();
  EXPECT_TRUE(view_->GetVisible());
}
