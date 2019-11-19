// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_DIALOG_VIEW_H_

#include "base/observer_list.h"
#include "base/optional.h"
#include "chrome/browser/ui/global_media_controls/media_dialog_delegate.h"
#include "chrome/browser/ui/global_media_controls/media_notification_container_observer.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class MediaDialogViewObserver;
class MediaNotificationContainerImplView;
class MediaNotificationListView;
class MediaNotificationService;

// Dialog that shows media controls that control the active media session.
class MediaDialogView : public views::BubbleDialogDelegateView,
                        public MediaDialogDelegate,
                        public MediaNotificationContainerObserver {
 public:
  static void ShowDialog(views::View* anchor_view,
                         MediaNotificationService* service);
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

  // views::DialogDelegate implementation.
  bool Close() override;

  // views::View implementation.
  void AddedToWidget() override;
  gfx::Size CalculatePreferredSize() const override;

  // MediaNotificationContainerObserver implementation.
  void OnContainerExpanded(bool expanded) override;
  void OnContainerMetadataChanged() override;
  void OnContainerClicked(const std::string& id) override {}
  void OnContainerDismissed(const std::string& id) override {}
  void OnContainerDestroyed(const std::string& id) override;
  void OnContainerDraggedOut(const std::string& id, gfx::Rect bounds) override {
  }

  void AddObserver(MediaDialogViewObserver* observer);
  void RemoveObserver(MediaDialogViewObserver* observer);

  const std::map<const std::string, MediaNotificationContainerImplView*>&
  GetNotificationsForTesting() const;

 private:
  explicit MediaDialogView(views::View* anchor_view,
                           MediaNotificationService* service);
  ~MediaDialogView() override;

  static MediaDialogView* instance_;

  // True if the dialog has been opened this session.
  static bool has_been_opened_;

  // views::BubbleDialogDelegateView implementation.
  void Init() override;
  void WindowClosing() override;

  MediaNotificationService* const service_;

  MediaNotificationListView* const active_sessions_view_;

  base::ObserverList<MediaDialogViewObserver> observers_;

  // A map of all containers we're currently observing.
  std::map<const std::string, MediaNotificationContainerImplView*>
      observed_containers_;

  DISALLOW_COPY_AND_ASSIGN(MediaDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_DIALOG_VIEW_H_
