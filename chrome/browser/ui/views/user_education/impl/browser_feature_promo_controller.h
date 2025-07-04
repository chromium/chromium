// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_FEATURE_PROMO_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_FEATURE_PROMO_CONTROLLER_H_

#include <concepts>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/user_education/browser_help_bubble.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/interaction/element_tracker_views.h"

// Wrapper for classes which implement some descendant of
// `FeaturePromoControllerCommon`. Provides overrides of methods common to all
// browser feature promo controllers. Derive your controller from this instead
// of writing it from scratch.
//
// (See `BrowserFeaturePromoController20` and `BrowserFeaturePromoController25`
// for examples.)
template <typename T>
  requires std::derived_from<T, user_education::FeaturePromoControllerCommon>
class BrowserFeaturePromoController : public T {
 public:
  template <typename... Args>
  explicit BrowserFeaturePromoController(BrowserView* browser_view,
                                         Args&&... args)
      : T(std::forward<Args>(args)...), browser_view_(browser_view) {
    CHECK(browser_view_);
  }
  ~BrowserFeaturePromoController() override = default;

  // FeaturePromoController:

  ui::ElementContext GetAnchorContext() const override {
    return views::ElementTrackerViews::GetContextForView(browser_view_);
  }

  const ui::AcceleratorProvider* GetAcceleratorProvider() const override {
    return browser_view_;
  }

  std::u16string GetTutorialScreenReaderHint() const override {
    return BrowserHelpBubble::GetFocusTutorialBubbleScreenReaderHint(
        GetAcceleratorProvider());
  }

  std::u16string GetFocusHelpBubbleScreenReaderHint(
      user_education::FeaturePromoSpecification::PromoType promo_type,
      ui::TrackedElement* anchor_element) const override {
    return BrowserHelpBubble::GetFocusHelpBubbleScreenReaderHint(
        promo_type, GetAcceleratorProvider(), anchor_element);
  }

  std::u16string GetBodyIconAltText() const override {
    return l10n_util::GetStringUTF16(IDS_CHROME_TIP);
  }

  const base::Feature* GetScreenReaderPromptPromoFeature() const override {
    return &feature_engagement::kIPHFocusHelpBubbleScreenReaderPromoFeature;
  }

  const char* GetScreenReaderPromptPromoEventName() const override {
    return feature_engagement::events::kFocusHelpBubbleAcceleratorPromoRead;
  }

 protected:
  BrowserView* browser_view() const { return browser_view_; }

 private:
  const raw_ptr<BrowserView> browser_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_FEATURE_PROMO_CONTROLLER_H_
