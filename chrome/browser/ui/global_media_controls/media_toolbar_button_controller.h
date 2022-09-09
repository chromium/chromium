// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_CONTROLLER_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "components/global_media_controls/public/media_item_manager_observer.h"

namespace global_media_controls {
class MediaItemManager;
}  // namespace global_media_controls

class MediaToolbarButtonControllerDelegate;

// Controller for the MediaToolbarButtonView that decides when to show or hide
// the icon from the toolbar.
class MediaToolbarButtonController
    : public global_media_controls::MediaItemManagerObserver {
 public:
  MediaToolbarButtonController(
      MediaToolbarButtonControllerDelegate* delegate,
      global_media_controls::MediaItemManager* item_manager);
  MediaToolbarButtonController(const MediaToolbarButtonController&) = delete;
  MediaToolbarButtonController& operator=(const MediaToolbarButtonController&) =
      delete;
  ~MediaToolbarButtonController() override;

  // global_media_controls::MediaItemManagerObserver:
  void OnItemListChanged() override;
  void OnMediaDialogOpened() override;
  void OnMediaDialogClosed() override;

  void ShowToolbarButton();

 private:
  // Tracks the current display state of the toolbar button delegate.
  enum class DisplayState {
    kShown,
    kDisabled,
    kHidden,
  };

  void UpdateToolbarButtonState();

  const raw_ptr<MediaToolbarButtonControllerDelegate> delegate_;
  const raw_ptr<global_media_controls::MediaItemManager> item_manager_;

  // The delegate starts hidden and isn't shown until media playback starts.
  DisplayState delegate_display_state_ = DisplayState::kHidden;
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_CONTROLLER_H_
