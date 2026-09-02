// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/user_education/user_education_handler.h"

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/webui/user_education_internals/user_education_internals_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/webui/user_education.mojom-shared.h"
#include "components/user_education/webui/user_education.mojom.h"
#include "components/webui/chrome_urls/pref_names.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_manager/user_names.h"
#endif

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kInstrumentedId);

class UserEducationHandlerBrowsertest : public InteractiveBrowserTest {
 public:
  UserEducationHandlerBrowsertest() = default;
  ~UserEducationHandlerBrowsertest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    g_browser_process->local_state()->SetBoolean(
        chrome_urls::kInternalOnlyUisEnabled, true);
  }

  static UserEducationMixedTrustHandler* GetHandler() {
    auto* el = ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
        kInstrumentedId);
    if (!el) {
      return nullptr;
    }
    auto* const web_el = AsInstrumentedWebContents(el);
    auto* const web_ui = static_cast<UserEducationInternalsUI*>(
        web_el->web_contents()->GetWebUI()->GetController());
    return web_ui->user_education_handler_.get();
  }

  auto RunCrashChecks() {
    return Steps(
        PollUntil([]() { return !!GetHandler(); }, "Waiting for handler."),
        WithElement(kInstrumentedId, [](ui::TrackedElement* el) {
          UNCALLED_MOCK_CALLBACK(
              user_education::mojom::UserEducationMixedTrustHandler::
                  MaybeShowNewBadgeForCallback,
              new_badge_callback);
          UserEducationMixedTrustHandler* handler = GetHandler();
          CHECK(handler);
          handler->NotifyAdditionalConditionEvent("doesn't exist");
          handler->NotifyFeaturePromoFeatureUsed(
              feature_engagement::kIPHWebUiHelpBubbleTestFeature.name,
              user_education::mojom::FeaturePromoFeatureUsedAction::
                  kClosePromoIfPresent);
          handler->NotifyNewBadgeFeatureUsed(
              user_education::features::kNewBadgeTestFeature.name);
          EXPECT_CALL_IN_SCOPE(
              new_badge_callback, Run(false),
              handler->MaybeShowNewBadgeFor(
                  user_education::features::kNewBadgeTestFeature.name,
                  new_badge_callback.Get()));
          handler->MaybeShowFeaturePromo(
              user_education::mojom::FeaturePromoParams::New(
                  feature_engagement::kIPHWebUiHelpBubbleTestFeature.name,
                  std::nullopt));
        }));
  }
};

// Regression tests for https://crbug.com/553802387.
// Verify that in off-the-record modes, one can call handler methods without
// crashing.

IN_PROC_BROWSER_TEST_F(UserEducationHandlerBrowsertest, NoCrashInIncognito) {
  auto* const incog = CreateIncognitoBrowser(browser()->GetProfile());
  RunTestSequence(InAnyContext(
      InstrumentTab(kInstrumentedId, std::nullopt, incog),
      NavigateWebContents(kInstrumentedId,
                          GURL(chrome::kChromeUIUserEducationInternalsURL)),
      RunCrashChecks()));
}

#if !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(UserEducationHandlerBrowsertest, NoCrashInGuest) {
  auto* const guest = CreateGuestBrowser();
  RunTestSequence(InAnyContext(
      InstrumentTab(kInstrumentedId, std::nullopt, guest),
      NavigateWebContents(kInstrumentedId,
                          GURL(chrome::kChromeUIUserEducationInternalsURL)),
      RunCrashChecks()));
}

#else

class UserEducationHandlerBrowsertestChromeOS
    : public UserEducationHandlerBrowsertest {
 public:
  UserEducationHandlerBrowsertestChromeOS() = default;
  ~UserEducationHandlerBrowsertestChromeOS() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    UserEducationHandlerBrowsertest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(ash::switches::kGuestSession);
    command_line->AppendSwitchASCII(ash::switches::kLoginUser,
                                    user_manager::kGuestUserName);
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile,
                                    TestingProfile::kTestUserProfileDir);
    command_line->AppendSwitch(switches::kIncognito);
  }
};

IN_PROC_BROWSER_TEST_F(UserEducationHandlerBrowsertestChromeOS,
                       NoCrashInGuest) {
  RunTestSequence(InAnyContext(
      InstrumentTab(kInstrumentedId),
      NavigateWebContents(kInstrumentedId,
                          GURL(chrome::kChromeUIUserEducationInternalsURL)),
      RunCrashChecks()));
}

#endif
