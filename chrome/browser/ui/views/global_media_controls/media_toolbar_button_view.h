// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller_delegate.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

class Browser;
class BrowserView;
class MediaNotificationService;
class MediaToolbarButtonController;
class MediaToolbarButtonObserver;
class MediaToolbarButtonContextualMenu;

// Media icon shown in the trusted area of toolbar. Its lifetime is tied to that
// of its parent ToolbarView. The icon is made visible when there is an active
// media session.
class MediaToolbarButtonView : public ToolbarButton,
                               public MediaToolbarButtonControllerDelegate {
  METADATA_HEADER(MediaToolbarButtonView, ToolbarButton)

 public:
  MediaToolbarButtonView(
      BrowserView* browser_view,
      std::unique_ptr<MediaToolbarButtonContextualMenu> context_menu);
  MediaToolbarButtonView(const MediaToolbarButtonView&) = delete;
  MediaToolbarButtonView& operator=(const MediaToolbarButtonView&) = delete;
  ~MediaToolbarButtonView() override;

  void AddObserver(MediaToolbarButtonObserver* observer);
  void RemoveObserver(MediaToolbarButtonObserver* observer);

  // MediaToolbarButtonControllerDelegate implementation.
  void Show() override;
  void Hide() override;
  void Enable() override;
  void Disable() override;
  void MaybeShowLocalMediaCastingPromo() override;
  void MaybeShowStopCastingPromo() override;

  MediaToolbarButtonController* media_toolbar_button_controller() {
    return controller_.get();
  }

 private:
  void ButtonPressed();
  void ClosePromoBubble(bool engaged);

  const raw_ptr<const Browser> browser_;

  const raw_ptr<MediaNotificationService> service_;

  std::unique_ptr<MediaToolbarButtonController> controller_;

  base::ObserverList<MediaToolbarButtonObserver> observers_;

  std::unique_ptr<MediaToolbarButtonContextualMenu> context_menu_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_VIEW_H_
