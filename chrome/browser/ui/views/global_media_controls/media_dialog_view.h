// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_DIALOG_VIEW_H_

#include <map>
#include <memory>
#include <string>

#include "base/observer_list.h"
#include "base/optional.h"
#include "chrome/browser/accessibility/soda_installer.h"
#include "chrome/browser/ui/global_media_controls/media_dialog_delegate.h"
#include "chrome/browser/ui/global_media_controls/media_notification_container_observer.h"
#include "chrome/browser/ui/views/global_media_controls/global_media_controls_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/metadata/metadata_header_macros.h"

class MediaDialogViewObserver;
class MediaNotificationContainerImplView;
class MediaNotificationListView;
class MediaNotificationService;
class NewBadgeLabel;
class Profile;

namespace views {
class Label;
class ToggleButton;
}  // namespace views

namespace content {
class WebContents;
}  // namespace content

// Dialog that shows media controls that control the active media session.
class MediaDialogView : public views::BubbleDialogDelegateView,
                        public MediaDialogDelegate,
                        public MediaNotificationContainerObserver,
                        public speech::SodaInstaller::Observer {
 public:
  METADATA_HEADER(MediaDialogView);

  MediaDialogView(const MediaDialogView&) = delete;
  MediaDialogView& operator=(const MediaDialogView&) = delete;

  static views::Widget* ShowDialog(views::View* anchor_view,
                                   MediaNotificationService* service,
                                   Profile* profile,
                                   GlobalMediaControlsEntryPoint entry_point);
  static views::Widget* ShowDialogForPresentationRequest(
      views::View* anchor_view,
      MediaNotificationService* service,
      Profile* profile,
      content::WebContents* contents,
      GlobalMediaControlsEntryPoint entry_point);
  static void HideDialog();
  static bool IsShowing();

  static MediaDialogView* GetDialogViewForTesting() { return instance_; }

  // MediaDialogDelegate implementation.
  MediaNotificationContainerImpl* ShowMediaSession(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item) override;
  void HideMediaSession(const std::string& id) override;
  std::unique_ptr<OverlayMediaNotification> PopOut(const std::string& id,
                                                   gfx::Rect bounds) override;
  void HideMediaDialog() override;

  // views::View implementation.
  void AddedToWidget() override;
  gfx::Size CalculatePreferredSize() const override;

  // MediaNotificationContainerObserver implementation.
  void OnContainerSizeChanged() override;
  void OnContainerMetadataChanged() override;
  void OnContainerActionsChanged() override;
  void OnContainerClicked(const std::string& id) override {}
  void OnContainerDismissed(const std::string& id) override {}
  void OnContainerDestroyed(const std::string& id) override;
  void OnContainerDraggedOut(const std::string& id, gfx::Rect bounds) override {
  }
  void OnAudioSinkChosen(const std::string& id,
                         const std::string& sink_id) override {}

  void AddObserver(MediaDialogViewObserver* observer);
  void RemoveObserver(MediaDialogViewObserver* observer);

  const std::map<const std::string, MediaNotificationContainerImplView*>&
  GetNotificationsForTesting() const;

  const MediaNotificationListView* GetListViewForTesting() const;

 private:
  friend class MediaDialogViewBrowserTest;
  MediaDialogView(views::View* anchor_view,
                  MediaNotificationService* service,
                  Profile* profile,
                  content::WebContents* contents,
                  GlobalMediaControlsEntryPoint entry_point);

  ~MediaDialogView() override;

  static MediaDialogView* instance_;

  // True if the dialog has been opened this session.
  static bool has_been_opened_;

  // views::BubbleDialogDelegateView implementation.
  void Init() override;
  void WindowClosing() override;

  // views::Button::PressedCallback
  void OnLiveCaptionButtonPressed();

  void ToggleLiveCaption(bool enabled);
  void UpdateBubbleSize();

  // SodaInstaller::Observer overrides:
  void OnSodaInstalled() override;
  void OnSodaError() override;
  void OnSodaProgress(int progress) override;

  MediaNotificationService* const service_;

  Profile* const profile_;

  MediaNotificationListView* const active_sessions_view_;

  base::ObserverList<MediaDialogViewObserver> observers_;

  // A map of all containers we're currently observing.
  std::map<const std::string, MediaNotificationContainerImplView*>
      observed_containers_;

  views::View* live_caption_container_ = nullptr;
  // TODO(crbug.com/1055150): Remove live_caption_title_new_badge_ by M93.
  NewBadgeLabel* live_caption_title_new_badge_ = nullptr;
  views::Label* live_caption_title_ = nullptr;
  views::ToggleButton* live_caption_button_ = nullptr;

  // It stores the WebContents* from which a MediaRouterDialogControllerViews
  // opened the dialog for a presentation request. It is nullptr if the dialog
  // is opened from the toolbar.
  content::WebContents* const web_contents_for_presentation_request_ = nullptr;
  const GlobalMediaControlsEntryPoint entry_point_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_DIALOG_VIEW_H_
