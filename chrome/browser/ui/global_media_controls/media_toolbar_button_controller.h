// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_CONTROLLER_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_CONTROLLER_H_

#include "chrome/browser/ui/global_media_controls/media_notification_service_observer.h"

class MediaNotificationService;
class MediaToolbarButtonControllerDelegate;

// Controller for the MediaToolbarButtonView that decides when to show or hide
// the icon from the toolbar.
class MediaToolbarButtonController : public MediaNotificationServiceObserver {
 public:
  MediaToolbarButtonController(MediaToolbarButtonControllerDelegate* delegate,
                               MediaNotificationService* service);
  MediaToolbarButtonController(const MediaToolbarButtonController&) = delete;
  MediaToolbarButtonController& operator=(const MediaToolbarButtonController&) =
      delete;
  ~MediaToolbarButtonController() override;

  // MediaNotificationServiceObserver implementation.
  void OnNotificationListChanged() override;
  void OnMediaDialogOpenedOrClosed() override;

 private:
  // Tracks the current display state of the toolbar button delegate.
  enum class DisplayState {
    kShown,
    kDisabled,
    kHidden,
  };

  void UpdateToolbarButtonState();

  MediaToolbarButtonControllerDelegate* const delegate_;
  MediaNotificationService* const service_;

  // The delegate starts hidden and isn't shown until media playback starts.
  DisplayState delegate_display_state_ = DisplayState::kHidden;
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_CONTROLLER_H_
