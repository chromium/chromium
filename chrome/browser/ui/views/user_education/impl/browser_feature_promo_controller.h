// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_FEATURE_PROMO_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_FEATURE_PROMO_CONTROLLER_H_

#include <string>

#include "base/feature_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/common/feature_promo/impl/feature_promo_controller_impl.h"
#include "components/user_education/common/feature_promo/impl/precondition_list_provider.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/view.h"

// Browser implementation of FeaturePromoController for User Education 2.5.
// There is one instance per browser window.
//
// This is implemented in c/b/ui/views specifically because some of the logic
// requires understanding of the existence of views, not because this is a
// views-specific implementation.
class BrowserFeaturePromoController
    : public user_education::FeaturePromoControllerImpl {
 public:
  using user_education::FeaturePromoControllerImpl::FeaturePromoControllerImpl;
  ~BrowserFeaturePromoController() override;

  // Returns the browser which is the primary window for `view`, or null if
  // there isn't one.
  static BrowserWindowInterface* GetBrowserForView(const views::View* view);

 protected:
  // user_education::FeaturePromoControllerImpl:
  void AddDemoPreconditionProviders(
      user_education::ComposingPreconditionListProvider& to_add_to,
      bool required) override;
  void AddPreconditionProviders(
      user_education::ComposingPreconditionListProvider& to_add_to,
      Priority priority,
      bool required) override;
  std::u16string GetTutorialScreenReaderHint(
      const ui::AcceleratorProvider* accelerator_provider) const override;
  std::u16string GetFocusHelpBubbleScreenReaderHint(
      user_education::FeaturePromoSpecification::PromoType promo_type,
      ui::TrackedElement* anchor_element,
      const ui::AcceleratorProvider* accelerator_provider) const override;
  std::u16string GetBodyIconAltText() const override;
  const base::Feature* GetScreenReaderPromptPromoFeature() const override;
  const char* GetScreenReaderPromptPromoEventName() const override;
  user_education::UserEducationContextPtr GetContextForHelpBubble(
      const ui::TrackedElement* anchor_element) const override;

 private:
  static user_education::UserEducationContextPtr GetContextForHelpBubbleImpl(
      const ui::TrackedElement* anchor_element);
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_FEATURE_PROMO_CONTROLLER_H_
