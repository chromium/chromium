// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller_delegate.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"

class Browser;
class GlobalMediaControlsPromoController;
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
  explicit MediaToolbarButtonView(const Browser* browser);
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

  // views::InkDropHostView implementation.
  SkColor GetInkDropBaseColor() const override;

  // Updates the icon image.
  void UpdateIcon();

  void ShowPromo();

  // Called when the in-product help bubble has gone away.
  void OnPromoEnded();

  GlobalMediaControlsPromoController* GetPromoControllerForTesting() {
    EnsurePromoController();
    return promo_controller_.get();
  }

 private:
  // Lazily constructs |promo_controller_| if necessary.
  void EnsurePromoController();

  // Informs the Global Media Controls in-product help that the GMC dialog was
  // opened.
  void InformIPHOfDialogShown();

  // Informs the Global Media Controls in-product help of the current button
  // state.
  void InformIPHOfButtonEnabled();
  void InformIPHOfButtonDisabledorHidden();

  // Shows the in-product help bubble.
  std::unique_ptr<GlobalMediaControlsPromoController> promo_controller_;

  // True if the in-product help bubble is currently showing.
  bool is_promo_showing_ = false;

  MediaNotificationService* const service_;
  std::unique_ptr<MediaToolbarButtonController> controller_;
  const Browser* const browser_;

  base::ObserverList<MediaToolbarButtonObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(MediaToolbarButtonView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_VIEW_H_
