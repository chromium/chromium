// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_FEATURE_PROMO_CONTROLLER_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_FEATURE_PROMO_CONTROLLER_BROWSERTEST_BASE_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_handle.h"
#include "components/user_education/common/feature_promo/feature_promo_registry.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/common/feature_promo/impl/feature_promo_controller_impl.h"
#include "components/user_education/common/help_bubble/help_bubble_factory_registry.h"
#include "components/user_education/common/user_education_context.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "components/user_education/test/user_education_session_test_util.h"
#include "components/user_education/views/help_bubble_view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"

namespace user_education {

namespace test {

// Test features commonly used across BrowserFeaturePromoController browser
// tests.
BASE_DECLARE_FEATURE(kTestIPHFeature);
BASE_DECLARE_FEATURE(kSnoozeIPHFeature);
BASE_DECLARE_FEATURE(kTutorialIPHFeature);
BASE_DECLARE_FEATURE(kCustomActionIPHFeature);
BASE_DECLARE_FEATURE(kDefaultCustomActionIPHFeature);
BASE_DECLARE_FEATURE(kLegalNoticeFeature);
BASE_DECLARE_FEATURE(kLegalNoticeFeature2);
BASE_DECLARE_FEATURE(kActionableAlertIPHFeature);
BASE_DECLARE_FEATURE(kActionableAlertIPHFeature2);
BASE_DECLARE_FEATURE(kKeyedPromoFeature);
BASE_DECLARE_FEATURE(kKeyedPromoFeature2);
BASE_DECLARE_FEATURE(kRotatingPromoIPHFeature);

inline constexpr char kTestTutorialIdentifier[] = "Test Tutorial";
inline constexpr char kAppName1[] = "app1";
inline constexpr char kAppName2[] = "app2";

DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kPromoShownEvent);

}  // namespace test

class BrowserFeaturePromoControllerTestBase : public InteractiveBrowserTest {
 public:
  BrowserFeaturePromoControllerTestBase();
  ~BrowserFeaturePromoControllerTestBase() override;

  // InteractiveBrowserTest / InProcessBrowserTest:
  void SetUp() override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

 protected:
  virtual void RegisterIPH();

  BrowserView* browser_view();

  StepBuilder ResetSessionData(base::TimeDelta since_session_start,
                               base::TimeDelta idle_time = base::Seconds(0));

  StepBuilder AdvanceTime(
      std::optional<base::TimeDelta> until_new_last_active,
      base::TimeDelta until_new_now = base::Milliseconds(500),
      bool send_update = true);

  MultiStep MaybeShowPromo(
      FeaturePromoParams params,
      FeaturePromoResult expected = FeaturePromoResult::Success(),
      std::optional<base::TimeDelta> timeout_delta = std::nullopt);

  MultiStep ClosePromo();

  MultiStep AbortPromo();

  MultiStep ExpectShowingPromo(const base::Feature* feature);

  MultiStep CheckPromoStatus(const base::Feature& iph_feature,
                             FeaturePromoStatus status);

  void VerifyConstants();

  FeaturePromoSpecification DefaultPromoSpecification(
      const base::Feature& feature);

  FeaturePromoControllerImpl* controller() { return controller_.get(); }
  UserEducationService* user_education_service();
  UserEducationStorageService* storage_service();
  FeaturePromoRegistry* registry();
  HelpBubbleFactoryRegistry* bubble_factory();
  testing::NiceMock<feature_engagement::test::MockTracker>* mock_tracker() {
    return mock_tracker_;
  }
  const UserEducationContextPtr& user_education_context() const {
    return user_education_context_;
  }
  test::UserEducationSessionTestUtil* test_util() { return test_util_.get(); }

  views::View* GetAnchorView();
  ui::TrackedElement* GetAnchorElement();

  int custom_callback_count() const { return custom_callback_count_; }

  const base::TimeDelta kLessThanGracePeriod;
  const base::TimeDelta kMoreThanGracePeriod;
  const base::TimeDelta kLessThanCooldown;
  const base::TimeDelta kMoreThanCooldown;
  const base::TimeDelta kMoreThanSnooze;
  const base::TimeDelta kLessThanAbortCooldown;
  const base::TimeDelta kMoreThanAbortCooldown;
  const base::TimeDelta kLessThanNewSession;
  const base::TimeDelta kMoreThanNewSession;

 private:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context);
  static std::unique_ptr<KeyedService> MakeTestTracker(
      content::BrowserContext* context);

  void ResetSessionDataImpl(base::TimeDelta since_session_start,
                            base::TimeDelta idle_time,
                            BrowserView* browser_view);

  void AdvanceTimeImpl(std::optional<base::TimeDelta> until_new_last_active,
                       base::TimeDelta until_new_now,
                       bool send_update);

  void OnCustomPromoAction(const base::Feature* feature,
                           const UserEducationContextPtr& context,
                           FeaturePromoHandle promo_handle);

  // ScopedFeatureList must be the first member to be initialized first and
  // destroyed last, preventing feature state changes during member destruction.
  base::test::ScopedFeatureList scoped_feature_list_;
  base::CallbackListSubscription create_services_subscription_;
  raw_ptr<FeaturePromoControllerImpl> controller_ = nullptr;
  UserEducationContextPtr user_education_context_;
  raw_ptr<testing::NiceMock<feature_engagement::test::MockTracker>>
      mock_tracker_ = nullptr;
  FeaturePromoControllerImpl::TestLock lock_;
  base::Time now_;
  std::unique_ptr<test::UserEducationSessionTestUtil> test_util_;
  int custom_callback_count_ = 0;
};

}  // namespace user_education

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IMPL_BROWSER_FEATURE_PROMO_CONTROLLER_BROWSERTEST_BASE_H_
