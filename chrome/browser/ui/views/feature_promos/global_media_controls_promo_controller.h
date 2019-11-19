// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FEATURE_PROMOS_GLOBAL_MEDIA_CONTROLS_PROMO_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_FEATURE_PROMOS_GLOBAL_MEDIA_CONTROLS_PROMO_CONTROLLER_H_

#include "chrome/browser/ui/global_media_controls/media_toolbar_button_observer.h"
#include "chrome/browser/ui/views/feature_promos/feature_promo_bubble_view.h"
#include "ui/views/widget/widget_observer.h"

class MediaToolbarButtonView;
class Profile;

// Handles display of the global media controls in-product help promo, including
// showing the promo bubble and highlighting the appropriate app menu items.
// Notifies the GlobalMediaControlsInProductHelp service when the promo is
// finished.
class GlobalMediaControlsPromoController : public views::WidgetObserver,
                                           public MediaToolbarButtonObserver {
 public:
  GlobalMediaControlsPromoController(MediaToolbarButtonView* owner,
                                     Profile* profile);
  ~GlobalMediaControlsPromoController() override = default;

  // Shows the IPH promo. Should only be called once.
  void ShowPromo();

  // MediaToolbarButtonObserver implementation.
  void OnMediaDialogOpened() override;
  void OnMediaButtonShown() override {}
  void OnMediaButtonHidden() override;
  void OnMediaButtonEnabled() override {}
  void OnMediaButtonDisabled() override;

  void disable_bubble_timeout_for_test() {
    disable_bubble_timeout_for_test_ = true;
  }

  views::View* promo_bubble_for_test() { return promo_bubble_; }

 private:
  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // Closes the dialog if necessary and informs the IPH service that the dialog
  // was closed.
  void FinishPromo();

  MediaToolbarButtonView* const owner_;
  Profile* const profile_;
  FeaturePromoBubbleView* promo_bubble_ = nullptr;

  // Whether we are showing the promo.
  bool is_showing_ = false;

  // Whether ShowPromo() has ever been called. It should only ever be called
  // once.
  bool show_promo_called_ = false;

  // Whether we want to disable timeout for test.
  bool disable_bubble_timeout_for_test_ = false;

  DISALLOW_COPY_AND_ASSIGN(GlobalMediaControlsPromoController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FEATURE_PROMOS_GLOBAL_MEDIA_CONTROLS_PROMO_CONTROLLER_H_
