// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_SELECTOR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_SELECTOR_VIEW_H_

#include "base/callback_list.h"
#include "chrome/browser/ui/global_media_controls/media_notification_device_provider.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "media/audio/audio_device_description.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/box_layout.h"

namespace {
class DeviceEntryView;
class ExpandDeviceSelectorButton;
}  // anonymous namespace

class MediaNotificationDeviceSelectorViewDelegate;

class MediaNotificationDeviceSelectorView
    : public views::View,
      public views::ButtonListener,
      public IconLabelBubbleView::Delegate {
 public:
  MediaNotificationDeviceSelectorView(
      MediaNotificationDeviceSelectorViewDelegate* delegate,
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

  views::Button* get_expand_button_for_testing();
  static std::string get_entry_label_for_testing(views::View* entry_view);
  static bool get_entry_is_highlighted_for_testing(views::View* entry_view);

 private:
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationDeviceSelectorViewTest,
                           DeviceButtonsCreated);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationDeviceSelectorViewTest,
                           ExpandButtonOpensEntryContainer);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationDeviceSelectorViewTest,
                           DeviceButtonClickNotifiesContainer);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationDeviceSelectorViewTest,
                           CurrentDeviceHighlighted);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationDeviceSelectorViewTest,
                           DeviceHighlightedOnChange);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationDeviceSelectorViewTest,
                           DeviceButtonsChange);

  void UpdateVisibility();

  bool ShouldBeVisible();

  void ShowDevices();
  void HideDevices();

  bool is_expanded_ = false;
  bool is_audio_device_switching_enabled_ = false;
  MediaNotificationDeviceSelectorViewDelegate* const delegate_;
  std::string current_device_id_;
  SkColor foreground_color_, background_color_;
  DeviceEntryView* current_device_entry_view_ = nullptr;

  // Child views
  views::View* expand_button_strip_;
  ExpandDeviceSelectorButton* expand_button_;
  views::View* audio_device_entries_container_;

  std::unique_ptr<MediaNotificationDeviceProvider::
                      GetOutputDevicesCallbackList::Subscription>
      audio_device_subscription_;

  std::unique_ptr<base::RepeatingCallbackList<void(bool)>::Subscription>
      is_device_switching_enabled_subscription_;

  base::WeakPtrFactory<MediaNotificationDeviceSelectorView> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_SELECTOR_VIEW_H_
