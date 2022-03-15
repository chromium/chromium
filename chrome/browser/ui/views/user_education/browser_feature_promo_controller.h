// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_BROWSER_FEATURE_PROMO_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_BROWSER_FEATURE_PROMO_CONTROLLER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/user_education/feature_promo_controller.h"
#include "chrome/browser/ui/user_education/feature_promo_specification.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_service.h"
#include "ui/base/interaction/element_identifier.h"

class BrowserView;
class FeaturePromoRegistry;
class FeaturePromoSnoozeService;
class HelpBubbleFactoryRegistry;
class TutorialService;

namespace feature_engagement {
class Tracker;
}

namespace ui {
class AcceleratorProvider;
class TrackedElement;
}  // namespace ui

namespace views {
class View;
}  // namespace views

// Browser implementation of FeaturePromoController. There is one instance per
// browser window.
//
// This is implemented in c/b/ui/views specifically because some of the logic
// requires understanding of the existence of views, not because this is a
// views-specific implementation.
class BrowserFeaturePromoController : public FeaturePromoControllerCommon {
 public:
  // Create the instance for the given |browser_view|.
  BrowserFeaturePromoController(
      BrowserView* browser_view,
      feature_engagement::Tracker* feature_engagement_tracker,
      FeaturePromoRegistry* registry,
      HelpBubbleFactoryRegistry* help_bubble_registry,
      FeaturePromoSnoozeService* snooze_service,
      TutorialService* tutorial_service);
  ~BrowserFeaturePromoController() override;

  // Get the appropriate instance for |view|. This finds the BrowserView
  // that contains |view| and returns its instance. May return nullptr,
  // but if |view| is in a BrowserView's hierarchy it shouldn't.
  static BrowserFeaturePromoController* GetForView(views::View* view);

  // Returns true if IPH are allowed to show in an inactive window or app.
  // False by default, but bunit tests may modify this behavior via
  // BlockActiveWindowCheckForTesting(). Exposed here for testing purposes.
  static bool active_window_check_blocked_for_testing() {
    return active_window_check_blocked();
  }

 protected:
  FRIEND_TEST_ALL_PREFIXES(BrowserFeaturePromoControllerTest, GetAnchorContext);
  FRIEND_TEST_ALL_PREFIXES(BrowserFeaturePromoControllerTest,
                           GetAcceleratorProvider);
  FRIEND_TEST_ALL_PREFIXES(BrowserFeaturePromoControllerTest,
                           GetFocusHelpBubbleScreenReaderHint);
  FRIEND_TEST_ALL_PREFIXES(BrowserFeaturePromoControllerUiTest, CanShowPromo);

  // FeaturePromoController:
  // Gets the context in which to locate the anchor view.
  ui::ElementContext GetAnchorContext() const override;

  // Determine if the current context and anchor element allow showing a promo.
  // This lets us rule out e.g. inactive and incognito windows for non-critical
  // promos.
  bool CanShowPromo(ui::TrackedElement* anchor_element) const override;

  // Get the accelerator provider to use to look up accelerators.
  const ui::AcceleratorProvider* GetAcceleratorProvider() const override;

  // These methods control how snooze buttons appear and function.
  std::u16string GetSnoozeButtonText() const override;
  std::u16string GetDismissButtonText() const override;

  // This method returns an appropriate prompt for promoting using a navigation
  // accelerator to focus the help bubble.
  std::u16string GetFocusHelpBubbleScreenReaderHint(
      FeaturePromoSpecification::PromoType promo_type,
      ui::TrackedElement* anchor_element,
      bool is_critical_promo) const override;

 private:
  // The browser window this instance is responsible for.
  const raw_ptr<BrowserView> browser_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_BROWSER_FEATURE_PROMO_CONTROLLER_H_
