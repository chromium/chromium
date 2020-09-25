// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller_delegate.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"

class Browser;
class BrowserView;
class FeaturePromoControllerViews;
class MediaNotificationService;
class MediaToolbarButtonController;
class MediaToolbarButtonObserver;

// Media icon shown in the trusted area of toolbar. Its lifetime is tied to that
// of its parent ToolbarView. The icon is made visible when there is an active
// media session.
class MediaToolbarButtonView : public ToolbarButton,
                               public MediaToolbarButtonControllerDelegate,
                               public views::ButtonListener {
 public:
  explicit MediaToolbarButtonView(BrowserView* browser_view);
  ~MediaToolbarButtonView() override;

  void AddObserver(MediaToolbarButtonObserver* observer);
  void RemoveObserver(MediaToolbarButtonObserver* observer);

  // MediaToolbarButtonControllerDelegate implementation.
  void Show() override;
  void Hide() override;
  void Enable() override;
  void Disable() override;

  // views::ButtonListener implementation.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // ToolbarButton implementation.
  void UpdateIcon() override;

 private:
  const Browser* const browser_;

  MediaNotificationService* const service_;

  // The window's IPH promo controller.
  FeaturePromoControllerViews* const feature_promo_controller_;

  std::unique_ptr<MediaToolbarButtonController> controller_;

  base::ObserverList<MediaToolbarButtonObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(MediaToolbarButtonView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_VIEW_H_
