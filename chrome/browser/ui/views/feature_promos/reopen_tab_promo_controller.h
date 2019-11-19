// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FEATURE_PROMOS_REOPEN_TAB_PROMO_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_FEATURE_PROMOS_REOPEN_TAB_PROMO_CONTROLLER_H_

#include "chrome/browser/ui/views/frame/app_menu_button_observer.h"
#include "ui/views/widget/widget_observer.h"

class BrowserView;
class FeaturePromoBubbleView;
class ReopenTabInProductHelp;

// Handles display of the reopen tab in-product help promo, including showing
// the promo bubble and highlighting the appropriate app menu items. Notifies
// the |ReopenTabInProductHelp| service when the promo is finished.
class ReopenTabPromoController : public AppMenuButtonObserver,
                                 public views::WidgetObserver {
 public:
  explicit ReopenTabPromoController(BrowserView* browser_view);
  ~ReopenTabPromoController() override = default;

  // Shows the IPH promo. Should only be called once.
  void ShowPromo();

  // Called when the user activates the entry with |command_id| in the "recent
  // tabs" menu.
  void OnTabReopened(int command_id);

  void disable_bubble_timeout_for_test() {
    disable_bubble_timeout_for_test_ = true;
  }

 private:
  // Last step of the flow completed by the user before dismissal (whether by
  // successful completion of the flow, timing out, or clicking away.). This is
  // used for an UMA histogram.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class StepAtDismissal {
    // The promo bubble was shown, but the menu was not opened; i.e. the bubble
    // timed out.
    kBubbleShown = 0,
    // The menu was opened, but the user clicked away without opening the last
    // closed tab.
    kMenuOpened = 1,
    // The last closed tab item was clicked. The promo was successful.
    kTabReopened = 2,

    kMaxValue = kTabReopened,
  };

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // AppMenuButtonObserver:
  void AppMenuShown() override;
  void AppMenuClosed() override;

  // Called when the promo flow ends, either because the bubble timed out, or
  // because the user did something to close the menu.
  void PromoEnded();

  ReopenTabInProductHelp* const iph_service_;
  BrowserView* const browser_view_;
  FeaturePromoBubbleView* promo_bubble_ = nullptr;

  // The promo stage that has been reached, logged to a histogram when the promo
  // flow ends.
  StepAtDismissal promo_step_ = StepAtDismissal::kBubbleShown;

  // Whether we are showing the promo.
  bool is_showing_ = false;

  // Whether ShowPromo() has ever been called. It should only ever be called
  // once.
  bool show_promo_called_ = false;

  // Whether we want to disable timeout for test.
  bool disable_bubble_timeout_for_test_ = false;

  DISALLOW_COPY_AND_ASSIGN(ReopenTabPromoController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FEATURE_PROMOS_REOPEN_TAB_PROMO_CONTROLLER_H_
