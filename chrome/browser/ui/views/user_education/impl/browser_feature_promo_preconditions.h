// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_FEATURE_PROMO_PRECONDITIONS_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_FEATURE_PROMO_PRECONDITIONS_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "ui/base/interaction/typed_data_collection.h"
#include "ui/events/event.h"
#include "ui/events/event_observer.h"
#include "ui/views/event_monitor.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

DECLARE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kWindowActivePrecondition);
DECLARE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(
    kContentNotFullscreenPrecondition);
DECLARE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(
    kOmniboxNotOpenPrecondition);
DECLARE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(
    kToolbarNotCollapsedPrecondition);
DECLARE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(
    kBrowserNotClosingPrecondition);
DECLARE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(
    kNoCriticalNoticeShowingPrecondition);
DECLARE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kUserNotActivePrecondition);

// Requires that the window a promo will be shown in is active.
class WindowActivePrecondition
    : public user_education::FeaturePromoPreconditionBase {
 public:
  WindowActivePrecondition();
  ~WindowActivePrecondition() override;

  // FeaturePromoPreconditionBase:
  user_education::FeaturePromoResult CheckPrecondition(
      ui::UnownedTypedDataCollection& data) const override;
};

// Requires that the window isn't in content-fullscreen.
class ContentNotFullscreenPrecondition
    : public user_education::FeaturePromoPreconditionBase {
 public:
  explicit ContentNotFullscreenPrecondition(Browser& browser);
  ~ContentNotFullscreenPrecondition() override;

  // FeaturePromoPreconditionBase:
  user_education::FeaturePromoResult CheckPrecondition(
      ui::UnownedTypedDataCollection& data) const override;

 private:
  const raw_ref<Browser> browser_;
};

// Precondition that the Omnibox isn't open.
class OmniboxNotOpenPrecondition
    : public user_education::FeaturePromoPreconditionBase {
 public:
  explicit OmniboxNotOpenPrecondition(const BrowserView& browser_view);
  ~OmniboxNotOpenPrecondition() override;

  // FeaturePromoPreconditionBase:
  user_education::FeaturePromoResult CheckPrecondition(
      ui::UnownedTypedDataCollection& data) const override;

 private:
  const raw_ref<const BrowserView> browser_view_;
};

// Trying to show an IPH when the toolbar is collapsed (Responsive Toolbar) is
// inadvisable, as the anchor may not be present, important UI for the feature
// might not be present (including tutorial elements), and the UI might be too
// small to properly accommodate a help bubble.
class ToolbarNotCollapsedPrecondition
    : public user_education::FeaturePromoPreconditionBase {
 public:
  explicit ToolbarNotCollapsedPrecondition(BrowserView& browser_view);
  ~ToolbarNotCollapsedPrecondition() override;

  // FeaturePromoPreconditionBase:
  user_education::FeaturePromoResult CheckPrecondition(
      ui::UnownedTypedDataCollection& data) const override;

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
      ui::UnownedTypedDataCollection& data) const override;

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
      ui::UnownedTypedDataCollection& data) const override;

 private:
  const raw_ref<BrowserView> browser_view_;
};

// Don't show heavyweight notices while the user is typing.
class UserNotActivePrecondition
    : public user_education::FeaturePromoPreconditionBase,
      public ui::EventObserver,
      public views::ViewObserver {
 public:
  explicit UserNotActivePrecondition(
      BrowserView& browser_view,
      const user_education::UserEducationTimeProvider& time_provider);
  ~UserNotActivePrecondition() override;

  // FeaturePromoPreconditionBase:
  user_education::FeaturePromoResult CheckPrecondition(
      ui::UnownedTypedDataCollection& data) const override;

 private:
  void CreateEventMonitor();

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override;

  // views::ViewObserver:
  void OnViewAddedToWidget(views::View* observed_view) override;
  void OnViewIsDeleting(views::View* observed_view) override;

  const raw_ref<BrowserView> browser_view_;
  const raw_ref<const user_education::UserEducationTimeProvider> time_provider_;
  std::unique_ptr<views::EventMonitor> event_monitor_;
  base::Time last_active_time_;
  base::ScopedObservation<views::View, views::ViewObserver>
      browser_view_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_FEATURE_PROMO_PRECONDITIONS_H_
