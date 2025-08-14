// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <string>
#include <string_view>

#include "base/notreached.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "components/user_education/common/ntp_promo/ntp_promo_registry.h"
#include "components/user_education/common/ntp_promo/ntp_promo_specification.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/common/user_education_metadata.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/typed_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/interaction/polling_view_observer.h"
#include "url/gurl.h"

namespace {

using user_education::features::NtpBrowserPromoType;
using Eligibility = user_education::NtpPromoSpecification::Eligibility;

inline constexpr char kTestPromoName[] = "test_promo";
const InteractiveBrowserTestApi::DeepQuery kPathToSimplePromo = {
    "ntp-app", "ntp-single-promo"};
const InteractiveBrowserTestApi::DeepQuery kPathToSetupList = {
    "ntp-app", "setup-list-module-wrapper", "setup-list", "setup-list-item"};
constexpr char kActionButtonId[] = "#actionButton";
constexpr char kActionTextId[] = "#bodyText";
constexpr char kActionIconId[] = "#bodyIcon";
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNtpElementId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kTestPromoShownEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kTestPromoClickedEvent);

}  // namespace

class NtpPromoUiTest : public InteractiveBrowserTest,
                       public testing::WithParamInterface<NtpBrowserPromoType> {
 public:
  NtpPromoUiTest() = default;
  ~NtpPromoUiTest() override = default;

  void SetUp() override {
    std::string param_value;
    switch (GetParam()) {
      case NtpBrowserPromoType::kSimple:
        param_value = "simple";
        break;
      case NtpBrowserPromoType::kSetupList:
        param_value = "setuplist";
        break;
      default:
        NOTREACHED();
    }
    feature_list_.InitAndEnableFeatureWithParameters(
        user_education::features::kEnableNtpBrowserPromos,
        {{user_education::features::kNtpBrowserPromoType.name, param_value}});
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    // Sanity check that the flag setup actually took; if it didn't, then we
    // can't accurately perform the test.
    ASSERT_EQ(user_education::features::GetNtpBrowserPromoType(), GetParam());
  }

  void InstallTestPromo(Eligibility eligibility) {
    UserEducationService* const service =
        UserEducationServiceFactory::GetForBrowserContext(browser()->profile());
    user_education::NtpPromoRegistry* registry = service->ntp_promo_registry();
    registry->ClearPromosForTesting();
    user_education::NtpPromoSpecification spec(
        kTestPromoName,
        user_education::NtpPromoContent("chrome-filled", IDS_NTP_SIGN_IN_PROMO,
                                        IDS_NTP_SIGN_IN_PROMO_ACTION_BUTTON),
        base::BindRepeating(&NtpPromoUiTest::GetTestPromoEligibility,
                            base::Unretained(this)),
        base::BindRepeating(&NtpPromoUiTest::OnTestPromoShown,
                            base::Unretained(this)),
        base::BindRepeating(&NtpPromoUiTest::OnTestPromoClicked,
                            base::Unretained(this)),
        {}, user_education::Metadata());
    registry->AddPromo(std::move(spec));

    test_promo_eligibility_ = eligibility;

    if (eligibility == Eligibility::kCompleted) {
      // Need to configure the data so that the completed promo can show.
      user_education::UserEducationStorageService& storage_service =
          service->user_education_storage_service();
      user_education::NtpPromoData data =
          storage_service.ReadNtpPromoData(kTestPromoName)
              .value_or(user_education::NtpPromoData());
      data.last_clicked = storage_service.GetCurrentTime();
      data.completed = storage_service.GetCurrentTime();
      storage_service.SaveNtpPromoData(kTestPromoName, data);
    }
  }

  auto GetFirstPromoPath() const {
    switch (GetParam()) {
      case NtpBrowserPromoType::kSimple:
        return kPathToSimplePromo;
      case NtpBrowserPromoType::kSetupList:
        return kPathToSetupList;
      default:
        NOTREACHED();
    }
  }

  auto GetActionButtonPath() const {
    return GetFirstPromoPath() + kActionButtonId;
  }

  auto GetActionIconPath() const { return GetFirstPromoPath() + kActionIconId; }

  auto WaitForActionIcon(std::string_view expected_icon) {
    const auto path = GetActionIconPath();
    auto steps = Steps(
        WaitForElementVisible(kNtpElementId, path),
        // Verify the icon shows the correct image.
        CheckJsResultAt(kNtpElementId, path, "el => el.icon", expected_icon));
    AddDescriptionPrefix(steps, __func__);
    return steps;
  }

  auto WaitForPromoVisible(Eligibility eligibility) {
    MultiStep steps;
    switch (eligibility) {
      case Eligibility::kEligible:
        steps += WaitForActionIcon("ntp-promo:chrome-filled");
        steps += WaitForElementVisible(kNtpElementId, GetActionButtonPath());
        break;
      case Eligibility::kCompleted:
        steps += WaitForActionIcon("ntp-promo:completed");
        steps += CheckJsResultAt(
            kNtpElementId, GetActionButtonPath(),
            "el => el && el.checkVisibility({visibilityProperty: true})",
            false);
        break;
      case Eligibility::kIneligible:
        NOTREACHED();
    }
    AddDescriptionPrefix(steps, __func__);
    return steps;
  }

  auto VerifyTestPromoText() {
    return CheckJsResultAt(kNtpElementId, GetFirstPromoPath() + kActionTextId,
                           "el => el.innerText",
                           l10n_util::GetStringUTF8(IDS_NTP_SIGN_IN_PROMO))
        .AddDescriptionPrefix(__func__);
  }

  auto PressActionButton() {
    return ClickElement(kNtpElementId, GetActionButtonPath())
        .AddDescriptionPrefix(__func__);
  }

 private:
  Eligibility GetTestPromoEligibility(Profile* profile) {
    return test_promo_eligibility_;
  }

  void OnTestPromoShown() {
    BrowserElements::From(browser())->NotifyEvent(kBrowserViewElementId,
                                                  kTestPromoShownEvent);
  }

  void OnTestPromoClicked(BrowserWindowInterface* window) {
    EXPECT_EQ(browser(), window);
    BrowserElements::From(window)->NotifyEvent(kBrowserViewElementId,
                                               kTestPromoClickedEvent);
  }

  Eligibility test_promo_eligibility_ = Eligibility::kEligible;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    NtpPromoUiTest,
    testing::Values(NtpBrowserPromoType::kSimple,
                    NtpBrowserPromoType::kSetupList),
    [](const testing::TestParamInfo<NtpBrowserPromoType>& info) {
      std::ostringstream oss;
      oss << info.param;
      return oss.str();
    });

IN_PROC_BROWSER_TEST_P(NtpPromoUiTest, TestPromoEligible) {
  InstallTestPromo(Eligibility::kEligible);
  RunTestSequence(
      InstrumentTab(kNtpElementId),
      // Because the "promo was shown" event is fired asynchronously as the page
      // is loading, watch for it in parallel with navigating to the NTP.
      InParallel(RunSubsequence(NavigateWebContents(
                     kNtpElementId, GURL(chrome::kChromeUINewTabPageURL))),
                 RunSubsequence(WaitForEvent(kBrowserViewElementId,
                                             kTestPromoShownEvent))),
      // Should already be visible at this point, but confirm it is and that it
      // is in the correct state.
      WaitForPromoVisible(Eligibility::kEligible), VerifyTestPromoText(),
      // As before, because the click and the event are sent asynchronously,
      // run these in parallel.
      InParallel(RunSubsequence(PressActionButton()),
                 RunSubsequence(WaitForEvent(kBrowserViewElementId,
                                             kTestPromoClickedEvent))));

  // TODD(https://crbug.com/433607240): Check model, histograms.
}

IN_PROC_BROWSER_TEST_P(NtpPromoUiTest, TestPromoCompleted) {
  // Single promo does not [yet] show completed promos.
  if (GetParam() == NtpBrowserPromoType::kSimple) {
    GTEST_SKIP();
  }

  InstallTestPromo(Eligibility::kCompleted);
  RunTestSequence(
      InstrumentTab(kNtpElementId),
      NavigateWebContents(kNtpElementId, GURL(chrome::kChromeUINewTabPageURL)),
      WaitForPromoVisible(Eligibility::kCompleted), VerifyTestPromoText());

  // TODD(https://crbug.com/433607240): Check model, histograms.
}

// Tests in this block rely on the fact that the top priority promotion is
// signin - except on ChromeOS, where there is no signin flow. So do not build
// or run these tests on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)

namespace {

using ObserverType =
    views::test::PollingViewPropertyObserver<std::u16string, OmniboxViewViews>;
DEFINE_LOCAL_TYPED_IDENTIFIER_VALUE(ObserverType, kLocationBarTextValue);
MATCHER_P(OptionalStringContains, text, "Optional string contains") {
  return arg.has_value() && arg.value().find(text) != std::u16string::npos;
}

}  // namespace

IN_PROC_BROWSER_TEST_P(NtpPromoUiTest, SigninPromoAppearsAndIsClickable) {
  RunTestSequence(
      InstrumentTab(kNtpElementId),
      NavigateWebContents(kNtpElementId, GURL(chrome::kChromeUINewTabPageURL)),
      WaitForPromoVisible(Eligibility::kEligible),

      // Since bots cannot navigate to actual pages, we can't use
      // WaitForWebContentsNavigation() or the like. Instead, verify that the
      // browser *tries* to navigate to the account login page.
      PollViewProperty(kLocationBarTextValue, kOmniboxElementId,
                       &OmniboxViewViews::GetText),
      // Click the promo button; this should navigate the current page.
      PressActionButton(),
      WaitForState(kLocationBarTextValue,
                   OptionalStringContains(u"accounts.google.com")));

  // TODD(https://crbug.com/433607240): Check model, histograms.
}

#endif
