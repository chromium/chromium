// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_FEATURE_PROMO_PRECONDITIONS_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_FEATURE_PROMO_PRECONDITIONS_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"

DECLARE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kWindowActivePrecondition);
DECLARE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(
    kOmniboxNotOpenPrecondition);
DECLARE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(
    kToolbarNotCollapsedPrecondition);
DECLARE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(
    kBrowserNotClosingPrecondition);
DECLARE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(
    kNoCriticalNoticeShowingPrecondition);

// Requires that the window a promo will be shown in is active.
class WindowActivePrecondition
    : public user_education::FeaturePromoPreconditionBase {
 public:
  WindowActivePrecondition();
  ~WindowActivePrecondition() override;

  // FeaturePromoPreconditionBase:
  user_education::FeaturePromoResult CheckPrecondition(
      ComputedData& data) const override;
};

// Precondition that the Omnibox isn't open.
class OmniboxNotOpenPrecondition
    : public user_education::FeaturePromoPreconditionBase {
 public:
  explicit OmniboxNotOpenPrecondition(const BrowserView& browser_view);
  ~OmniboxNotOpenPrecondition() override;

  // FeaturePromoPreconditionBase:
  user_education::FeaturePromoResult CheckPrecondition(
      ComputedData& data) const override;

 private:
  const raw_ref<const BrowserView> browser_view_;
};

class ToolbarNotCollapsedPrecondition
    : public user_education::FeaturePromoPreconditionBase {
 public:
  explicit ToolbarNotCollapsedPrecondition(BrowserView& browser_view);
  ~ToolbarNotCollapsedPrecondition() override;

  // FeaturePromoPreconditionBase:
  user_education::FeaturePromoResult CheckPrecondition(
      ComputedData& data) const override;

 private:
  const raw_ref<BrowserView> browser_view_;
};

// Trying to show an IPH while the browser is closing can cause problems; see
// https://crbug.com/346461762 for an example. This can also crash unit_tests
// that use a BrowserWindow but not a browser, so also check if the browser
// view's widget is closing.
class BrowserNotClosingPrecondition
    : public user_education::FeaturePromoPreconditionBase {
 public:
  explicit BrowserNotClosingPrecondition(BrowserView& browser_view);
  ~BrowserNotClosingPrecondition() override;

  // FeaturePromoPreconditionBase:
  user_education::FeaturePromoResult CheckPrecondition(
      ComputedData& data) const override;

 private:
  const raw_ref<BrowserView> browser_view_;
};

// Certain critical notices do not (yet) use the Product Messaging Service.
// TODO(https://crbug.com/324785292): When the migration is done, remove this
// precondition.
class NoCriticalNoticeShowingPrecondition
    : public user_education::FeaturePromoPreconditionBase {
 public:
  explicit NoCriticalNoticeShowingPrecondition(BrowserView& browser_view);
  ~NoCriticalNoticeShowingPrecondition() override;

  // FeaturePromoPreconditionBase:
  user_education::FeaturePromoResult CheckPrecondition(
      ComputedData& data) const override;

 private:
  const raw_ref<BrowserView> browser_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_FEATURE_PROMO_PRECONDITIONS_H_
