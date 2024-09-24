// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/low_usage_help_controller.h"

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/user_education/browser_feature_promo_storage_service.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/user_education/common/feature_promo_data.h"
#include "components/user_education/common/feature_promo_session_manager.h"
#include "components/user_education/common/feature_promo_storage_service.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"

class LowUsageHelpControllerBrowsertest : public InteractiveFeaturePromoTest {
 public:
  LowUsageHelpControllerBrowsertest()
      : InteractiveFeaturePromoTest(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHDesktopReEngagementFeature})) {}

  ~LowUsageHelpControllerBrowsertest() override = default;

  DECLARE_CLASS_CUSTOM_ELEMENT_EVENT_TYPE(kStartupSessionEvent);

  UserEducationService& GetUserEducationService() {
    auto* const service =
        UserEducationServiceFactory::GetForBrowserContext(browser()->profile());
    CHECK(service);
    return *service;
  }

  user_education::FeaturePromoStatus GetFeaturePromoStatus() const {
    return browser()
        ->window()
        ->GetFeaturePromoControllerForTesting()
        ->GetPromoStatus(feature_engagement::kIPHDesktopReEngagementFeature);
  }

  auto WaitForStartupSession() {
    return Steps(
        // Send an event when there is a new session, then wait for the event.
        Do([this]() {
          auto& session_manager =
              GetUserEducationService().feature_promo_session_manager();
          if (session_manager.new_session_since_startup()) {
            SendSessionEvent();
          } else {
            session_subscription_ =
                session_manager.AddNewSessionCallback(base::BindRepeating(
                    &LowUsageHelpControllerBrowsertest::SendSessionEvent,
                    base::Unretained(this)));
          }
        }),
        WaitForEvent(kBrowserViewElementId, kStartupSessionEvent));
  }

  auto VerifyPromoShown() {
    return Steps(
        WaitForShow(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),

        CheckResult([this]() { return GetFeaturePromoStatus(); },
                    user_education::FeaturePromoStatus::kBubbleShowing));
  }

 private:
  void SendSessionEvent() {
    views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
        kStartupSessionEvent, BrowserView::GetBrowserViewForBrowser(browser()));
    session_subscription_ = base::CallbackListSubscription();
  }

  base::CallbackListSubscription session_subscription_;
};

DEFINE_CLASS_CUSTOM_ELEMENT_EVENT_TYPE(LowUsageHelpControllerBrowsertest,
                                       kStartupSessionEvent);

IN_PROC_BROWSER_TEST_F(LowUsageHelpControllerBrowsertest,
                       NoPromoOnFreshProfile) {
  // Ensure that the controller has been created.
  EXPECT_NE(nullptr, LowUsageHelpController::GetForProfileForTesting(
                         browser()->profile()));

  RunTestSequence(
      // Processing new sessions happens on a one-frame delay, so clear the call
      // stack and let an IPH trigger.

      // Verify that the IPH has not been requested.
      CheckResult([this]() { return GetFeaturePromoStatus(); },
                  user_education::FeaturePromoStatus::kNotRunning));
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

IN_PROC_BROWSER_TEST_F(LowUsageHelpControllerBrowsertest, PromoOnNewSession) {
  RunTestSequence(
      // Trigger a new session artificially.
      Do([this]() {
        GetUserEducationService()
            .recent_session_observer()
            ->NotifyLowUsageSession();
      }),
      VerifyPromoShown());
}

IN_PROC_BROWSER_TEST_F(LowUsageHelpControllerBrowsertest, PRE_PromoAtStartup) {
  auto& storage_service = static_cast<BrowserFeaturePromoStorageService&>(
      GetUserEducationService().feature_promo_storage_service());

  const auto now = base::Time::Now();

  // Create a sparse recent session record, with the most recent session being
  // a day ago.
  RecentSessionData recent_session_data;
  recent_session_data.enabled_time = now - base::Days(120);
  recent_session_data.recent_session_start_times = {now - base::Days(19)};

  // Mirror similar information in the recent session data so that a new
  // session will be triggered on browser startup.
  user_education::FeaturePromoSessionData session_data;
  session_data.start_time = now - base::Days(1);
  session_data.most_recent_active_time = now - base::Hours(23);

  RunTestSequence(
      // Ensure that any session stuff happens before we try to manipulate the
      // data store.
      WaitForStartupSession(),
      // Write the fake data into the storage service.
      Do([&storage_service, session_data, recent_session_data]() {
        storage_service.SaveSessionData(session_data);
        storage_service.SaveRecentSessionData(recent_session_data);
      }));
}

IN_PROC_BROWSER_TEST_F(LowUsageHelpControllerBrowsertest, PromoAtStartup) {
  RunTestSequence(
      // A new session should be triggered at startup.
      WaitForStartupSession(), VerifyPromoShown());
}

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
