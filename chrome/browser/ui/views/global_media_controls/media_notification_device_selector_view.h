// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_SELECTOR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_SELECTOR_VIEW_H_

#include "base/callback_list.h"
#include "chrome/browser/ui/global_media_controls/media_notification_device_provider.h"
#include "chrome/browser/ui/media_router/cast_dialog_controller.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_device_entry_ui.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "media/audio/audio_device_description.h"

namespace {
class ExpandDeviceSelectorButton;
const char kAudioDevicesCountHistogramName[] =
    "Media.GlobalMediaControls.NumberOfAvailableAudioDevices";
const char kDeviceSelectorAvailableHistogramName[] =
    "Media.GlobalMediaControls.DeviceSelectorAvailable";
const char kDeviceSelectorOpenedHistogramName[] =
    "Media.GlobalMediaControls.DeviceSelectorOpened";
}  // anonymous namespace

class MediaNotificationDeviceSelectorViewDelegate;

class MediaNotificationDeviceSelectorView
    : public views::View,
      public views::ButtonListener,
      public IconLabelBubbleView::Delegate,
      public media_router::CastDialogController::Observer {
 public:
  MediaNotificationDeviceSelectorView(
      MediaNotificationDeviceSelectorViewDelegate* delegate,
      std::unique_ptr<media_router::CastDialogController> controller,
      const std::string& current_device_id,
      const SkColor& foreground_color,
      const SkColor& background_color);
  ~MediaNotificationDeviceSelectorView() override;

  // Called when audio output devices are discovered.
  void UpdateAvailableAudioDevices(
      const media::AudioDeviceDescriptions& device_descriptions);
  // Called when an audio device switch has occurred
  void UpdateCurrentAudioDevice(const std::string& current_device_id);
  void OnColorsChanged(const SkColor& foreground_color,
                       const SkColor& background_color);

  // Called when the audio device switching has become enabled or disabled.
  void UpdateIsAudioDeviceSwitchingEnabled(bool enabled);

  // views::ButtonListener
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // IconLabelBubbleView::Delegate
  SkColor GetIconLabelBubbleSurroundingForegroundColor() const override;
  SkColor GetIconLabelBubbleBackgroundColor() const override;

  //  media_router::CastDialogController::Observer
  void OnModelUpdated(const media_router::CastDialogModel& model) override;
  void OnControllerInvalidated() override;

  views::Button* GetExpandButtonForTesting();
  std::string GetEntryLabelForTesting(views::View* entry_view);
  bool GetEntryIsHighlightedForTesting(views::View* entry_view);

 private:
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationDeviceSelectorViewTest,
                           DeviceButtonsCreated);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationDeviceSelectorViewTest,
                           ExpandButtonOpensEntryContainer);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationDeviceSelectorViewTest,
                           AudioDeviceButtonClickNotifiesContainer);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationDeviceSelectorViewTest,
                           CurrentAudioDeviceHighlighted);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationDeviceSelectorViewTest,
                           AudioDeviceHighlightedOnChange);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationDeviceSelectorViewTest,
                           AudioDeviceButtonsChange);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationDeviceSelectorViewTest,
                           AudioDevicesCountHistogramRecorded);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationDeviceSelectorViewTest,
                           DeviceSelectorOpenedHistogramRecorded);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationDeviceSelectorViewTest,
                           CastDeviceButtonClickStartsCasting);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationDeviceSelectorViewTest,
                           CastDeviceButtonClickClearsIssue);

  void UpdateVisibility();
  bool ShouldBeVisible() const;
  void ShowDevices();
  void HideDevices();
  void RemoveDevicesOfType(DeviceEntryUIType type);
  void StartCastSession(CastDeviceEntryView* entry);
  DeviceEntryUI* GetDeviceEntryUI(views::View* view) const;

  bool has_expand_button_been_shown_ = false;
  bool have_devices_been_shown_ = false;

  bool is_expanded_ = false;
  bool is_audio_device_switching_enabled_ = false;
  MediaNotificationDeviceSelectorViewDelegate* const delegate_;
  std::string current_device_id_;
  SkColor foreground_color_, background_color_;
  AudioDeviceEntryView* current_audio_device_entry_view_ = nullptr;

  // Child views
  views::View* expand_button_strip_ = nullptr;
  ExpandDeviceSelectorButton* expand_button_ = nullptr;
  views::View* device_entry_views_container_ = nullptr;

  std::unique_ptr<MediaNotificationDeviceProvider::
                      GetOutputDevicesCallbackList::Subscription>
      audio_device_subscription_;
  std::unique_ptr<base::RepeatingCallbackList<void(bool)>::Subscription>
      is_device_switching_enabled_subscription_;

  std::unique_ptr<media_router::CastDialogController> cast_controller_;

  // Each button has a unique tag, which is used to look up DeviceEntryUI* in
  // |device_entry_ui_map_|.
  int next_tag_ = 0;
  std::map<int, DeviceEntryUI*> device_entry_ui_map_;

  base::WeakPtrFactory<MediaNotificationDeviceSelectorView> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_SELECTOR_VIEW_H_
