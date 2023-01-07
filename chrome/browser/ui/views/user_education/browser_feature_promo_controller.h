// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_BROWSER_FEATURE_PROMO_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_BROWSER_FEATURE_PROMO_CONTROLLER_H_

#include <string>
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "ui/base/interaction/element_identifier.h"

namespace feature_engagement {
class Tracker;
}

namespace ui {
class AcceleratorProvider;
class TrackedElement;
}  // namespace ui

namespace user_education {
class FeaturePromoRegistry;
class FeaturePromoSnoozeService;
class HelpBubbleFactoryRegistry;
class TutorialService;
}  // namespace user_education

namespace views {
class View;
}

class BrowserView;

// Browser implementation of FeaturePromoController. There is one instance per
// browser window.
//
// This is implemented in c/b/ui/views specifically because some of the logic
// requires understanding of the existence of views, not because this is a
// views-specific implementation.
class BrowserFeaturePromoController
    : public user_education::FeaturePromoControllerCommon {
 public:
  // Create the instance for the given |browser_view|.
  BrowserFeaturePromoController(
      BrowserView* browser_view,
      feature_engagement::Tracker* feature_engagement_tracker,
      user_education::FeaturePromoRegistry* registry,
      user_education::HelpBubbleFactoryRegistry* help_bubble_registry,
      user_education::FeaturePromoSnoozeService* snooze_service,
      user_education::TutorialService* tutorial_service);
  ~BrowserFeaturePromoController() override;

  // Get the appropriate instance for |view|. This finds the BrowserView
  // that contains |view| and returns its instance. May return nullptr,
  // but if |view| is in a BrowserView's hierarchy it shouldn't.
  static BrowserFeaturePromoController* GetForView(views::View* view);

  // Returns true if IPH are allowed to show in an inactive window or app.
  // False by default, but unit tests may modify this behavior via
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
  ui::ElementContext GetAnchorContext() const override;
  bool CanShowPromo(ui::TrackedElement* anchor_element) const override;
  const ui::AcceleratorProvider* GetAcceleratorProvider() const override;
  std::u16string GetTutorialScreenReaderHint() const override;
  std::u16string GetFocusHelpBubbleScreenReaderHint(
      user_education::FeaturePromoSpecification::PromoType promo_type,
      ui::TrackedElement* anchor_element,
      bool is_critical_promo) const override;
  std::u16string GetBodyIconAltText() const override;
  const base::Feature* GetScreenReaderPromptPromoFeature() const override;
  const char* GetScreenReaderPromptPromoEventName() const override;

 private:
  // The browser window this instance is responsible for.
  const raw_ptr<BrowserView> browser_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_BROWSER_FEATURE_PROMO_CONTROLLER_H_
