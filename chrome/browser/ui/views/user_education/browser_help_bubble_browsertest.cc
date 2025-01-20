// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/browser_help_bubble.h"

#include <string>

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_tracker.h"

class BrowserHelpBubbleBrowsertest : public InteractiveBrowserTest {
 public:
  BrowserHelpBubbleBrowsertest() = default;
  ~BrowserHelpBubbleBrowsertest() override = default;
};

IN_PROC_BROWSER_TEST_F(BrowserHelpBubbleBrowsertest,
                       GetFocusHelpBubbleScreenReaderHint_Toast) {
  RunTestSequence(CheckElement(
      kToolbarAppMenuButtonElementId,
      [this](ui::TrackedElement* el) {
        return BrowserHelpBubble::GetFocusHelpBubbleScreenReaderHint(
            user_education::FeaturePromoSpecification::PromoType::kToast,
            BrowserView::GetBrowserViewForBrowser(browser()), el);
      },
      testing::Eq(std::u16string())));
}

IN_PROC_BROWSER_TEST_F(BrowserHelpBubbleBrowsertest,
                       GetFocusHelpBubbleScreenReaderHint_Abridged) {
  RunTestSequence(CheckElement(
      kTabIconElementId,
      [this](ui::TrackedElement* el) {
        return BrowserHelpBubble::GetFocusHelpBubbleScreenReaderHint(
            user_education::FeaturePromoSpecification::PromoType::kSnooze,
            BrowserView::GetBrowserViewForBrowser(browser()), el);
      },
      testing::Ne(std::u16string())));
}

IN_PROC_BROWSER_TEST_F(BrowserHelpBubbleBrowsertest,
                       GetFocusHelpBubbleScreenReaderHint_Tutorial) {
  RunTestSequence(CheckElement(
      kToolbarAppMenuButtonElementId,
      [this](ui::TrackedElement* el) {
        return BrowserHelpBubble::GetFocusHelpBubbleScreenReaderHint(
            user_education::FeaturePromoSpecification::PromoType::kTutorial,
            BrowserView::GetBrowserViewForBrowser(browser()), el);
      },
      testing::Ne(std::u16string())));
}

IN_PROC_BROWSER_TEST_F(BrowserHelpBubbleBrowsertest,
                       GetFocusTutorialBubbleScreenReaderHint) {
  RunTestSequence(CheckElement(
      kToolbarAppMenuButtonElementId,
      [this](ui::TrackedElement* el) {
        return BrowserHelpBubble::GetFocusTutorialBubbleScreenReaderHint(
            BrowserView::GetBrowserViewForBrowser(browser()));
      },
      testing::Ne(std::u16string())));
}

IN_PROC_BROWSER_TEST_F(BrowserHelpBubbleBrowsertest,
                       GetFocusBubbleAcceleratorText) {
  RunTestSequence(CheckView(
      kBrowserViewElementId,
      [](BrowserView* browser_view) {
        return BrowserHelpBubble::GetFocusBubbleAcceleratorText(browser_view);
      },
      testing::Ne(std::u16string())));
}
