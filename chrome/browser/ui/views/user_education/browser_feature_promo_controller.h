// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_BROWSER_FEATURE_PROMO_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_BROWSER_FEATURE_PROMO_CONTROLLER_H_

#include <memory>
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
class FeaturePromoSessionPolicy;
class FeaturePromoStorageService;
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
  // Create the instance for the given |browser_view|. Prefer to call
  // `MaybeCreateForBrowserView()` instead.
  BrowserFeaturePromoController(
      BrowserView* browser_view,
      feature_engagement::Tracker* feature_engagement_tracker,
      user_education::FeaturePromoRegistry* registry,
      user_education::HelpBubbleFactoryRegistry* help_bubble_registry,
      user_education::FeaturePromoStorageService* storage_service,
      user_education::FeaturePromoSessionPolicy* session_policy,
      user_education::TutorialService* tutorial_service);
  ~BrowserFeaturePromoController() override;

  // Creates (or doesn't create) a FeaturePromoController for the specified
  // `browser_view`. Not all browser windows can do promos; specifically,
  // headless, kiosk, guest, incognito, and other off-the-record browsers do
  // _not_ show IPH.
  static std::unique_ptr<BrowserFeaturePromoController>
  MaybeCreateForBrowserView(BrowserView* browser_view);

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
  FRIEND_TEST_ALL_PREFIXES(BrowserFeaturePromoControllerUiTest,
                           CanShowPromoForElement);

  // FeaturePromoController:
  ui::ElementContext GetAnchorContext() const override;
  bool CanShowPromoForElement(
      ui::TrackedElement* anchor_element) const override;
  const ui::AcceleratorProvider* GetAcceleratorProvider() const override;
  std::u16string GetTutorialScreenReaderHint() const override;
  std::u16string GetFocusHelpBubbleScreenReaderHint(
      user_education::FeaturePromoSpecification::PromoType promo_type,
      ui::TrackedElement* anchor_element,
      bool is_critical_promo) const override;
  std::u16string GetBodyIconAltText() const override;
  const base::Feature* GetScreenReaderPromptPromoFeature() const override;
  const char* GetScreenReaderPromptPromoEventName() const override;
  std::string GetAppId() const override;

 private:
  // The browser window this instance is responsible for.
  const raw_ptr<BrowserView> browser_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_BROWSER_FEATURE_PROMO_CONTROLLER_H_
