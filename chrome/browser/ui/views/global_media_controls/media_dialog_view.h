// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_DIALOG_VIEW_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/global_media_controls/live_translate_combobox_model.h"
#include "components/global_media_controls/public/constants.h"
#include "components/global_media_controls/public/media_dialog_delegate.h"
#include "components/global_media_controls/public/media_item_ui_observer.h"
#include "components/media_message_center/notification_theme.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class PrefChangeRegistrar;
class RichHoverButton;
class MediaDialogViewObserver;
class MediaNotificationService;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace global_media_controls {
class MediaItemUIListView;
class MediaItemUIUpdatedView;
class MediaItemUIView;
}  // namespace global_media_controls

namespace views {
class Combobox;
class Label;
class Separator;
class ToggleButton;
}  // namespace views

// Dialog that shows media controls that control the active media session.
class MediaDialogView : public views::BubbleDialogDelegateView,
                        public global_media_controls::MediaDialogDelegate,
                        public global_media_controls::MediaItemUIObserver,
                        public speech::SodaInstaller::Observer {
  METADATA_HEADER(MediaDialogView, views::BubbleDialogDelegateView)
 public:

  MediaDialogView(const MediaDialogView&) = delete;
  MediaDialogView& operator=(const MediaDialogView&) = delete;

  static views::Widget* ShowDialogFromToolbar(views::View* anchor_view,
                                              MediaNotificationService* service,
                                              Profile* profile);
  static views::Widget* ShowDialogCentered(
      const gfx::Rect& bounds,
      MediaNotificationService* service,
      Profile* profile,
      content::WebContents* contents,
      global_media_controls::GlobalMediaControlsEntryPoint entry_point);
  static views::Widget* ShowDialog(
      views::View* anchor_view,
      views::BubbleBorder::Arrow anchor_position,
      MediaNotificationService* service,
      Profile* profile,
      content::WebContents* contents,
      global_media_controls::GlobalMediaControlsEntryPoint entry_point);

  static void HideDialog();
  static bool IsShowing();

  static MediaDialogView* GetDialogViewForTesting() { return instance_; }

  // global_media_controls::MediaDialogDelegate:
  global_media_controls::MediaItemUI* ShowMediaItem(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item) override;
  void HideMediaItem(const std::string& id) override;
  void RefreshMediaItem(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item) override;
  void HideMediaDialog() override;
  void Focus() override;

  // views::View implementation.
  void AddedToWidget() override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // global_media_controls::MediaItemUIObserver implementation.
  void OnMediaItemUISizeChanged() override;
  void OnMediaItemUIMetadataChanged() override;
  void OnMediaItemUIActionsChanged() override;
  void OnMediaItemUIDestroyed(const std::string& id) override;

  void AddObserver(MediaDialogViewObserver* observer);
  void RemoveObserver(MediaDialogViewObserver* observer);

  void TargetLanguageChanged();

  const std::map<
      const std::string,
      raw_ptr<global_media_controls::MediaItemUIView, CtnExperimental>>&
  GetItemsForTesting() const;

  const std::map<
      const std::string,
      raw_ptr<global_media_controls::MediaItemUIUpdatedView, CtnExperimental>>&
  GetUpdatedItemsForTesting() const;

  const global_media_controls::MediaItemUIListView* GetListViewForTesting()
      const;

 private:
  friend class MediaDialogViewBrowserTest;
  friend class MediaDialogViewWithRemotePlaybackTest;

  MediaDialogView(
      views::View* anchor_view,
      views::BubbleBorder::Arrow anchor_position,
      MediaNotificationService* service,
      Profile* profile,
      content::WebContents* contents,
      global_media_controls::GlobalMediaControlsEntryPoint entry_point);

  ~MediaDialogView() override;

  static MediaDialogView* instance_;

  // True if the dialog has been opened this session.
  static bool has_been_opened_;

  // views::BubbleDialogDelegateView implementation.
  void Init() override;
  void WindowClosing() override;

  // views::Button::PressedCallback
  void OnLiveCaptionButtonPressed();
  void OnLiveTranslateButtonPressed();
  void OnSettingsButtonPressed();

  void UpdateBubbleSize();

  void OnLiveCaptionEnabledChanged();
  void OnLiveTranslateEnabledChanged();

  // SodaInstaller::Observer overrides:
  void OnSodaInstalled(speech::LanguageCode language_code) override;
  void OnSodaInstallError(speech::LanguageCode language_code,
                          speech::SodaInstaller::ErrorCode error_code) override;
  void OnSodaProgress(speech::LanguageCode language_code,
                      int progress) override;

  void InitializeLiveCaptionSection();
  void InitializeLiveTranslateSection();
  void InitializeCaptionSettingsSection();
  void SetLiveCaptionTitle(const std::u16string& new_text);

  std::unique_ptr<global_media_controls::MediaItemUIView> BuildMediaItemUIView(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item);
  std::unique_ptr<global_media_controls::MediaItemUIUpdatedView>
  BuildMediaItemUIUpdatedView(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item);

  const raw_ptr<MediaNotificationService> service_;

  const raw_ptr<Profile> profile_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  const raw_ptr<global_media_controls::MediaItemUIListView>
      active_sessions_view_;

  base::ObserverList<MediaDialogViewObserver> observers_;

  // A map of all the media item UIs that `MediaDialogView` is currently
  // observing. If media::kGlobalMediaControlsUpdatedUI on non-CrOS is enabled,
  // `updated_items_` is used, otherwise `observed_items_` is used.
  std::map<const std::string,
           raw_ptr<global_media_controls::MediaItemUIView, CtnExperimental>>
      observed_items_;
  std::map<
      const std::string,
      raw_ptr<global_media_controls::MediaItemUIUpdatedView, CtnExperimental>>
      updated_items_;

  raw_ptr<views::View> live_caption_container_ = nullptr;
  raw_ptr<views::Label> live_caption_title_ = nullptr;
  raw_ptr<views::ToggleButton> live_caption_button_ = nullptr;

  raw_ptr<views::Separator> separator_ = nullptr;
  raw_ptr<views::View> live_translate_container_ = nullptr;
  raw_ptr<views::View> live_translate_label_wrapper_ = nullptr;
  raw_ptr<views::Label> live_translate_title_ = nullptr;
  raw_ptr<views::ToggleButton> live_translate_button_ = nullptr;
  raw_ptr<views::View> live_translate_settings_container_ = nullptr;

  raw_ptr<views::View> target_language_container_ = nullptr;
  raw_ptr<views::Combobox> target_language_combobox_ = nullptr;

  raw_ptr<RichHoverButton> caption_settings_button_ = nullptr;
  raw_ptr<views::View> caption_settings_container_ = nullptr;

  // It stores the WebContents* from which a MediaRouterDialogControllerViews
  // opened the dialog for a presentation request. It is nullptr if the dialog
  // is opened from the toolbar.
  const raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged>
      web_contents_for_presentation_request_ = nullptr;
  const global_media_controls::GlobalMediaControlsEntryPoint entry_point_;

  // Only sets colors for the updated UI if it is enabled.
  std::optional<media_message_center::MediaColorTheme> media_color_theme_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_DIALOG_VIEW_H_
