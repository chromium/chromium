// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_FEATURE_PROMO_CONTROLLER_20_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_FEATURE_PROMO_CONTROLLER_20_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/user_education/impl/browser_feature_promo_controller.h"
#include "components/user_education/common/feature_promo/impl/feature_promo_controller_20.h"
#include "ui/base/interaction/element_identifier.h"

namespace ui {
class TrackedElement;
}  // namespace ui

// Browser implementation of FeaturePromoController for User Education 20.
// There is one instance per browser window.
//
// This is implemented in c/b/ui/views specifically because some of the logic
// requires understanding of the existence of views, not because this is a
// views-specific implementation.
class BrowserFeaturePromoController20
    : public BrowserFeaturePromoController<
          user_education::FeaturePromoController20> {
 public:
  // Create the instance for the given |browser_view|. Prefer to call
  // `MaybeCreateForBrowserView()` instead.
  using BrowserFeaturePromoController::BrowserFeaturePromoController;
  ~BrowserFeaturePromoController20() override;

 protected:
  friend class BrowserFeaturePromoController20CanShowPromoForElementUiTest;

  // FeaturePromoController:
  user_education::FeaturePromoResult CanShowPromoForElement(
      ui::TrackedElement* anchor_element,
      const user_education::UserEducationContextPtr& context) const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_FEATURE_PROMO_CONTROLLER_20_H_
