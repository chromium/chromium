// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_FEATURE_PROMO_CONTROLLER_25_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_FEATURE_PROMO_CONTROLLER_25_H_

#include "chrome/browser/ui/views/user_education/impl/browser_feature_promo_controller.h"
#include "components/user_education/common/feature_promo/impl/feature_promo_controller_25.h"
#include "components/user_education/common/feature_promo/impl/precondition_list_provider.h"
#include "ui/base/interaction/element_identifier.h"

// Browser implementation of FeaturePromoController for User Education 2.5.
// There is one instance per browser window.
//
// This is implemented in c/b/ui/views specifically because some of the logic
// requires understanding of the existence of views, not because this is a
// views-specific implementation.
class BrowserFeaturePromoController25
    : public BrowserFeaturePromoController<
          user_education::FeaturePromoController25> {
 public:
  // Create the instance for the given |browser_view|. Prefer to call
  // `MaybeCreateForBrowserView()` instead.
  using BrowserFeaturePromoController::BrowserFeaturePromoController;
  ~BrowserFeaturePromoController25() override;

 protected:
  // FeaturePromoController25:
  void AddDemoPreconditionProviders(
      user_education::ComposingPreconditionListProvider& to_add_to,
      bool required) override;
  void AddPreconditionProviders(
      user_education::ComposingPreconditionListProvider& to_add_to,
      Priority priority,
      bool required) override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_FEATURE_PROMO_CONTROLLER_25_H_
