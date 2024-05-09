// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/dice_web_signin_interception_bubble_view.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/web_signin_interceptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/profiles/profile_colors_util.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/profile_destruction_waiter.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/event_constants.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

#include <string>

namespace {

// Returns the avatar button, which is the anchor view for the interception
// bubble.
AvatarToolbarButton* GetAvatarButton(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  AvatarToolbarButton* avatar_button =
      browser_view->toolbar_button_provider()->GetAvatarToolbarButton();
  DCHECK(avatar_button);
  return avatar_button;
}

enum class NameFormat { Regular, LongName, LongNameSingleWord };

struct TestParam {
  std::string test_suffix = "";
  WebSigninInterceptor::SigninInterceptionType interception_type =
      WebSigninInterceptor::SigninInterceptionType::kMultiUser;
  policy::EnterpriseManagementAuthority management_authority =
      policy::EnterpriseManagementAuthority::NONE;
  // Note: changes strings for kEnterprise type, otherwise adds badge on pic.
  bool is_intercepted_account_managed = false;
  bool use_dark_theme = false;
  SkColor4f intercepted_profile_color = SkColors::kLtGray;
  SkColor4f primary_profile_color = SkColors::kBlue;
  bool enable_webui_refresh = false;
  bool with_explicit_browser_signin_design = false;
  NameFormat name_format = NameFormat::Regular;
};

// To be passed as 4th argument to `INSTANTIATE_TEST_SUITE_P()`, allows the test
// to be named like `All/<TestClassName>.InvokeUi_default/<TestSuffix>` instead
// of using the index of the param in `kTestParam` as suffix.
std::string ParamToTestSuffix(const ::testing::TestParamInfo<TestParam>& info) {
  return info.param.test_suffix;
}

// Permutations of supported bubbles.
const TestParam kTestParams[] = {
    // Common consumer user case: regular account signing in to a profile having
    // a regular account on a non-managed device.
    {
        .test_suffix = "ConsumerSimple",
        .interception_type =
            WebSigninInterceptor::SigninInterceptionType::kMultiUser,
        .intercepted_profile_color = SkColors::kMagenta,
    },

    // Ditto, with ChromeRefresh2023.
    {
        .test_suffix = "ConsumerSimpleChromeRefresh2023",
        .interception_type =
            WebSigninInterceptor::SigninInterceptionType::kMultiUser,
        .intercepted_profile_color = SkColors::kMagenta,
        .enable_webui_refresh = true,
        .with_explicit_browser_signin_design = true,
    },

    // Ditto, with a different color scheme
    {
        .test_suffix = "ConsumerDark",
        .interception_type =
            WebSigninInterceptor::SigninInterceptionType::kMultiUser,
        .use_dark_theme = true,
        .intercepted_profile_color = SkColors::kMagenta,
    },

    // Regular account signing in to a profile having a regular account on a
    // managed device (having policies configured locally for example).
    {
        .test_suffix = "ConsumerManagedDevice",
        .interception_type =
            WebSigninInterceptor::SigninInterceptionType::kMultiUser,
        .management_authority =
            policy::EnterpriseManagementAuthority::COMPUTER_LOCAL,
        .intercepted_profile_color = SkColors::kYellow,
        .primary_profile_color = SkColors::kMagenta,
    },

    // Regular account signing in to a profile having a managed account on a
    // non-managed device.
    {
        .test_suffix = "EnterpriseSimple",
        .interception_type =
            WebSigninInterceptor::SigninInterceptionType::kEnterprise,
    },

    // Managed account signing in to a profile having a regular account on a
    // non-managed device.
    {
        .test_suffix = "EnterpriseManagedIntercepted",
        .interception_type =
            WebSigninInterceptor::SigninInterceptionType::kEnterprise,
        .is_intercepted_account_managed = true,
    },

    // Ditto, with a different color scheme
    {
        .test_suffix = "EnterpriseManagedInterceptedDark",
        .interception_type =
            WebSigninInterceptor::SigninInterceptionType::kEnterprise,
        .is_intercepted_account_managed = true,
        .use_dark_theme = true,
    },

    // Regular account signing in to a profile having a managed account on a
    // managed device.
    {
        .test_suffix = "EntepriseManagedDevice",
        .interception_type =
            WebSigninInterceptor::SigninInterceptionType::kEnterprise,
        .management_authority =
            policy::EnterpriseManagementAuthority::CLOUD_DOMAIN,
    },

    // Profile switch bubble: the account used for signing in is already
    // associated with another profile.
    {
        .test_suffix = "ProfileSwitch",
        .interception_type =
            WebSigninInterceptor::SigninInterceptionType::kProfileSwitch,
    },

    // Ditto with ChromeRefresh2023.
    {
        .test_suffix = "ProfileSwitchChromeRefresh2023",
        .interception_type =
            WebSigninInterceptor::SigninInterceptionType::kProfileSwitch,
        .enable_webui_refresh = true,
        .with_explicit_browser_signin_design = true,
    },

    // Chrome Signin bubble: no accounts in chrome, and signing triggers this
    // intercept bubble.
    {
        .test_suffix = "ChromeSignin",
        .interception_type =
            WebSigninInterceptor::SigninInterceptionType::kChromeSignin,
    },
    {
        .test_suffix = "ChromeSigninDarkMode",
        .interception_type =
            WebSigninInterceptor::SigninInterceptionType::kChromeSignin,
        .use_dark_theme = true,
    },
    {
        .test_suffix = "ChromeSigninWebUIRefresh",
        .interception_type =
            WebSigninInterceptor::SigninInterceptionType::kChromeSignin,
        .enable_webui_refresh = true,
    },

    {
        .test_suffix = "ChromeSigninDarkModeWebUIRefresh",
        .interception_type =
            WebSigninInterceptor::SigninInterceptionType::kChromeSignin,
        .use_dark_theme = true,
        .enable_webui_refresh = true,
    },

    {
        .test_suffix = "ChromeSigninLongName",
        .interception_type =
            WebSigninInterceptor::SigninInterceptionType::kChromeSignin,
        .name_format = NameFormat::LongName,
    },

    {
        .test_suffix = "ChromeSigninLongNameSingleWord",
        .interception_type =
            WebSigninInterceptor::SigninInterceptionType::kChromeSignin,
        .name_format = NameFormat::LongNameSingleWord,
    },
};

}  // namespace

class DiceWebSigninInterceptionBubblePixelTest
    : public DialogBrowserTest,
      public testing::WithParamInterface<TestParam> {
 public:
  DiceWebSigninInterceptionBubblePixelTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    if (GetParam().enable_webui_refresh) {
      enabled_features.push_back(features::kChromeRefresh2023);
    }

    if (GetParam().with_explicit_browser_signin_design) {
      enabled_features.push_back(switches::kExplicitBrowserSigninUIOnDesktop);
    }

    scoped_feature_list_.InitWithFeatures(enabled_features,
                                          /*disabled_features=*/{});
  }

  // DialogBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (GetParam().use_dark_theme) {
      command_line->AppendSwitch(switches::kForceDarkMode);
    }
  }

  void ShowUi(const std::string& name) override {
    policy::ScopedManagementServiceOverrideForTesting browser_management(
        policy::ManagementServiceFactory::GetForProfile(browser()->profile()),
        GetParam().management_authority);
    policy::ScopedManagementServiceOverrideForTesting
        platform_browser_management(
            policy::ManagementServiceFactory::GetForPlatform(),
            policy::EnterpriseManagementAuthority::NONE);

    SkColor primary_highlight_color =
        GetParam().primary_profile_color.toSkColor();
    ProfileThemeColors colors = {
        /*profile_highlight_color=*/primary_highlight_color,
        /*default_avatar_fill_color=*/primary_highlight_color,
        /*default_avatar_stroke_color=*/
        GetAvatarStrokeColor(*browser()->window()->GetColorProvider(),
                             primary_highlight_color)};
    ProfileAttributesEntry* entry =
        g_browser_process->profile_manager()
            ->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(browser()->profile()->GetPath());
    DCHECK(entry);
    entry->SetProfileThemeColors(colors);

    std::string expected_intercept_url_string =
        GetParam().interception_type ==
                WebSigninInterceptor::SigninInterceptionType::kChromeSignin
            ? chrome::kChromeUIDiceWebSigninInterceptChromeSigninURL
            : chrome::kChromeUIDiceWebSigninInterceptURL;

    content::TestNavigationObserver observer{
        GURL(expected_intercept_url_string)};
    observer.StartWatchingNewWebContents();

    views::NamedWidgetShownWaiter widget_waiter(
        views::test::AnyWidgetTestPasskey{},
        "DiceWebSigninInterceptionBubbleView");

    bubble_handle_ = DiceWebSigninInterceptionBubbleView::CreateBubble(
        browser(), GetAvatarButton(browser()), GetTestBubbleParameters(),
        base::DoNothing());

    widget_waiter.WaitIfNeededAndGet();
    observer.Wait();
  }

  std::string GivenNameFromNameFormat() {
    switch (GetParam().name_format) {
      case NameFormat::Regular:
        return "Sam";
      case NameFormat::LongName:
        return "Sam With A Very Very Very Long Name";
      case NameFormat::LongNameSingleWord:
        return "SamWithAVeryVeryVeryVeryLongName";
    }
  }

  // Generates bubble parameters for testing.
  WebSigninInterceptor::Delegate::BubbleParameters GetTestBubbleParameters() {
    AccountInfo intercepted_account;
    intercepted_account.account_id =
        CoreAccountId::FromGaiaId("intercepted_ID");
    intercepted_account.given_name = GivenNameFromNameFormat();
    intercepted_account.full_name = intercepted_account.given_name + " Sample";
    intercepted_account.email = "sam.sample@intercepted.com";
    intercepted_account.hosted_domain =
        GetParam().is_intercepted_account_managed ? "intercepted.com"
                                                  : kNoHostedDomainFound;

    // `kEnterprise` type bubbles are used when at least one of the accounts is
    // managed. Instead of explicitly specifying it in the test parameters, we
    // can infer whether the primary account should be managed based on this,
    // since no test config has both accounts being managed.
    bool is_primary_account_managed =
        GetParam().interception_type ==
            WebSigninInterceptor::SigninInterceptionType::kEnterprise &&
        !GetParam().is_intercepted_account_managed;
    AccountInfo primary_account;
    primary_account.account_id = CoreAccountId::FromGaiaId("primary_ID");
    primary_account.given_name = "Tessa";
    primary_account.full_name = "Tessa Tester";
    primary_account.email = "tessa.tester@primary.com";
    primary_account.hosted_domain =
        is_primary_account_managed ? "primary.com" : kNoHostedDomainFound;
    bool show_managed_disclaimer =
        (GetParam().is_intercepted_account_managed ||
         GetParam().management_authority !=
             policy::EnterpriseManagementAuthority::NONE);

    return {GetParam().interception_type,
            intercepted_account,
            primary_account,
            GetParam().intercepted_profile_color.toSkColor(),
            /*show_link_data_option=*/false,
            show_managed_disclaimer};
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle> bubble_handle_;
};

// TODO(https://crbug.com/339315678): re-enable the test.
#if BUILDFLAG(IS_WIN)
#define MAYBE_InvokeUi_default DISABLED_InvokeUi_default
#else
#define MAYBE_InvokeUi_default InvokeUi_default
#endif
IN_PROC_BROWSER_TEST_P(DiceWebSigninInterceptionBubblePixelTest,
                       MAYBE_InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(All,
                         DiceWebSigninInterceptionBubblePixelTest,
                         testing::ValuesIn(kTestParams),
                         &ParamToTestSuffix);
