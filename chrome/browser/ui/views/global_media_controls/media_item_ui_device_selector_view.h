// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_UI_DEVICE_SELECTOR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_UI_DEVICE_SELECTOR_VIEW_H_

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/global_media_controls/media_notification_device_provider.h"
#include "chrome/browser/ui/media_router/cast_dialog_controller.h"
#include "chrome/browser/ui/views/global_media_controls/media_item_ui_footer_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_device_entry_ui.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "components/global_media_controls/public/constants.h"
#include "components/global_media_controls/public/views/media_item_ui_device_selector.h"
#include "components/media_router/common/media_sink.h"
#include "media/audio/audio_device_description.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace {
class ExpandDeviceSelectorLabel;
class ExpandDeviceSelectorButton;

const char kAudioDevicesCountHistogramName[] =
    "Media.GlobalMediaControls.NumberOfAvailableAudioDevices";
const char kCastDeviceCountHistogramName[] =
    "Media.GlobalMediaControls.CastDeviceCount";
const char kDeviceSelectorAvailableHistogramName[] =
    "Media.GlobalMediaControls.DeviceSelectorAvailable";
const char kDeviceSelectorOpenedHistogramName[] =
    "Media.GlobalMediaControls.DeviceSelectorOpened";
}  // anonymous namespace

namespace global_media_controls {
class MediaItemUIView;
}  // namespace global_media_controls

namespace media_router {
class CastDialogSinkButton;
}

class MediaItemUIDeviceSelectorDelegate;
class MediaItemUIDeviceSelectorObserver;

class MediaItemUIDeviceSelectorView
    : public global_media_controls::MediaItemUIDeviceSelector,
      public IconLabelBubbleView::Delegate,
      public media_router::CastDialogController::Observer,
      public MediaItemUIFooterView::Delegate {
 public:
  METADATA_HEADER(MediaItemUIDeviceSelectorView);
  MediaItemUIDeviceSelectorView(
      const std::string& item_id,
      MediaItemUIDeviceSelectorDelegate* delegate,
      std::unique_ptr<media_router::CastDialogController> cast_controller,
      bool has_audio_output,
      global_media_controls::GlobalMediaControlsEntryPoint entry_point,
      bool show_expand_button = true);
  ~MediaItemUIDeviceSelectorView() override;

  // Called when audio output devices are discovered.
  void UpdateAvailableAudioDevices(
      const media::AudioDeviceDescriptions& device_descriptions);

  // global_media_controls::MediaItemUIDeviceSelector:
  void SetMediaItemUIView(
      global_media_controls::MediaItemUIView* view) override;
  void OnColorsChanged(SkColor foreground_color,
                       SkColor background_color) override;
  void UpdateCurrentAudioDevice(const std::string& current_device_id) override;

  // Called when the audio device switching has become enabled or disabled.
  void UpdateIsAudioDeviceSwitchingEnabled(bool enabled);

  // IconLabelBubbleView::Delegate
  SkColor GetIconLabelBubbleSurroundingForegroundColor() const override;
  SkColor GetIconLabelBubbleBackgroundColor() const override;

  //  media_router::CastDialogController::Observer
  void OnModelUpdated(const media_router::CastDialogModel& model) override;
  void OnControllerInvalidated() override;

  // MediaItemUIFooterView::Delegate
  void OnDeviceSelected(int tag) override;
  void OnDropdownButtonClicked() override;
  bool IsDeviceSelectorExpanded() override;

  // views::View
  bool OnMousePressed(const ui::MouseEvent& event) override;

  void AddObserver(MediaItemUIDeviceSelectorObserver* observer);

  views::Label* GetExpandDeviceSelectorLabelForTesting();
  views::Button* GetDropdownButtonForTesting();
  std::string GetEntryLabelForTesting(views::View* entry_view);
  bool GetEntryIsHighlightedForTesting(views::View* entry_view);
  bool GetDeviceEntryViewVisibilityForTesting();
  std::vector<media_router::CastDialogSinkButton*>
  GetCastSinkButtonsForTesting();

 private:
  friend class MediaItemUIDeviceSelectorViewTest;
  FRIEND_TEST_ALL_PREFIXES(MediaItemUIDeviceSelectorViewTest,
                           AudioDevicesCountHistogramRecorded);
  FRIEND_TEST_ALL_PREFIXES(MediaItemUIDeviceSelectorViewTest,
                           DeviceSelectorOpenedHistogramRecorded);

  void UpdateVisibility();
  bool ShouldBeVisible() const;
  void CreateExpandButtonStrip(bool show_expand_button);
  void ShowOrHideDeviceList();
  void ShowDevices();
  void HideDevices();
  void RemoveDevicesOfType(DeviceEntryUIType type);
  void StartCastSession(CastDeviceEntryView* entry);
  void DoStartCastSession(media_router::UIMediaSink sink);
  void RecordCastDeviceCountAfterDelay();
  void RecordCastDeviceCount();
  DeviceEntryUI* GetDeviceEntryUI(views::View* view) const;
  void RegisterAudioDeviceCallbacks();

  bool has_expand_button_been_shown_ = false;
  bool have_devices_been_shown_ = false;

  bool is_expanded_ = false;
  bool is_audio_device_switching_enabled_ = false;
  bool has_cast_device_ = false;
  const std::string item_id_;
  const raw_ptr<MediaItemUIDeviceSelectorDelegate> delegate_;
  std::string current_device_id_ =
      media::AudioDeviceDescription::kDefaultDeviceId;
  SkColor foreground_color_ = global_media_controls::kDefaultForegroundColor;
  SkColor background_color_ = global_media_controls::kDefaultBackgroundColor;
  global_media_controls::GlobalMediaControlsEntryPoint const entry_point_;

  // Child views
  raw_ptr<AudioDeviceEntryView> current_audio_device_entry_view_ = nullptr;
  raw_ptr<views::View> expand_button_strip_ = nullptr;
  raw_ptr<ExpandDeviceSelectorLabel> expand_label_ = nullptr;
  raw_ptr<ExpandDeviceSelectorButton> dropdown_button_ = nullptr;
  raw_ptr<views::View> device_entry_views_container_ = nullptr;

  base::CallbackListSubscription audio_device_subscription_;
  base::CallbackListSubscription is_device_switching_enabled_subscription_;

  raw_ptr<global_media_controls::MediaItemUIView> media_item_ui_ = nullptr;

  std::unique_ptr<media_router::CastDialogController> cast_controller_;

  base::ObserverList<MediaItemUIDeviceSelectorObserver> observers_;

  // Each button has a unique tag, which is used to look up DeviceEntryUI* in
  // |device_entry_ui_map_|.
  int next_tag_ = 0;
  std::map<int, DeviceEntryUI*> device_entry_ui_map_;

  base::WeakPtrFactory<MediaItemUIDeviceSelectorView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_UI_DEVICE_SELECTOR_VIEW_H_
