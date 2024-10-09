// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

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
#include "chrome/browser/ui/views/profiles/dice_web_signin_interception_bubble_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/profile_destruction_waiter.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/supervised_user/core/common/features.h"
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

enum class ManagedAccountState : int {
  kNonManagedAccount = 0,
  kEnterpriseAccount = 1,
  kSupervisedAccount = 2
};

struct TestParam {
  std::string test_suffix = "";
  WebSigninInterceptor::SigninInterceptionType interception_type =
      WebSigninInterceptor::SigninInterceptionType::kMultiUser;
  policy::EnterpriseManagementAuthority management_authority =
      policy::EnterpriseManagementAuthority::NONE;
  // Note: changes strings for kEnterprise type, otherwise adds badge on pic.
  ManagedAccountState intercepted_account_management_state =
      ManagedAccountState::kNonManagedAccount;
  ManagedAccountState primary_account_management_state =
      ManagedAccountState::kNonManagedAccount;

  bool use_dark_theme = false;
  SkColor4f intercepted_profile_color = SkColors::kLtGray;
  SkColor4f primary_profile_color = SkColors::kBlue;
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

    // Ditto, with explicit browser signin.
    {
        .test_suffix = "ConsumerSimpleExplicitBrowserSignin",
        .interception_type =
            WebSigninInterceptor::SigninInterceptionType::kMultiUser,
        .intercepted_profile_color = SkColors::kMagenta,
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
        .primary_account_management_state =
            ManagedAccountState::kEnterpriseAccount,
    },
    // Managed account signing in to a profile having a regular account on a
    // non-managed device.
    {
        .test_suffix = "EnterpriseManagedIntercepted",
        .interception_type =
            WebSigninInterceptor::SigninInterceptionType::kEnterprise,
        .intercepted_account_management_state =
            ManagedAccountState::kEnterpriseAccount,
    },

    // Ditto, with a different color scheme
    {
        .test_suffix = "EnterpriseManagedInterceptedDark",
        .interception_type =
            WebSigninInterceptor::SigninInterceptionType::kEnterprise,
        .intercepted_account_management_state =
            ManagedAccountState::kEnterpriseAccount,
        .use_dark_theme = true,
    },

    // Supervised user sign-in intercept bubble, when user signs in in secondary
    // profile.
    {
        .test_suffix = "SecondaryProfileSupervisedIntercepted",
        .interception_type =
            WebSigninInterceptor::SigninInterceptionType::kMultiUser,
        .intercepted_account_management_state =
            ManagedAccountState::kSupervisedAccount,
    },
    {.test_suffix =
         "SecondaryProfileSupervisedInterceptedFromPrimaryEnterprize",
     .interception_type =
         WebSigninInterceptor::SigninInterceptionType::kMultiUser,
     .intercepted_account_management_state =
         ManagedAccountState::kSupervisedAccount,
     .primary_account_management_state =
         ManagedAccountState::kEnterpriseAccount},

    // Regular account signing in to a profile having a managed account on a
    // managed device.
    {.test_suffix = "EntepriseManagedDevice",
     .interception_type =
         WebSigninInterceptor::SigninInterceptionType::kEnterprise,
     .management_authority =
         policy::EnterpriseManagementAuthority::CLOUD_DOMAIN,
     .primary_account_management_state =
         ManagedAccountState::kEnterpriseAccount},

    // Profile switch bubble: the account used for signing in is already
    // associated with another profile.
    {
        .test_suffix = "ProfileSwitch",
        .interception_type =
            WebSigninInterceptor::SigninInterceptionType::kProfileSwitch,
    },

    // Ditto, with explicit browser signin.
    {
        .test_suffix = "ProfileSwitchExplicitBrowserSignin",
        .interception_type =
            WebSigninInterceptor::SigninInterceptionType::kProfileSwitch,
        .with_explicit_browser_signin_design = true,
    },

    // Supervised user sign-in intercept bubble, no accounts in chrome.
    {
        .test_suffix = "ChromeSignInSupervisedUserIntercepted",
        .interception_type =
            WebSigninInterceptor::SigninInterceptionType::kChromeSignin,
        .intercepted_account_management_state =
            ManagedAccountState::kSupervisedAccount,
    },
    // Profile switch for supervised user.
    {
        .test_suffix = "SupervisedUserProfileSwitch",
        .interception_type =
            WebSigninInterceptor::SigninInterceptionType::kProfileSwitch,
        .intercepted_account_management_state =
            ManagedAccountState::kSupervisedAccount,
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
    if (GetParam().with_explicit_browser_signin_design) {
      enabled_features.push_back(switches::kExplicitBrowserSigninUIOnDesktop);
    }
    enabled_features.push_back(
        supervised_user::kCustomProfileStringsForSupervisedUsers);
    enabled_features.push_back(supervised_user::kShowKiteForSupervisedUsers);
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
    DefaultAvatarColors avatar_colors = GetDefaultAvatarColors(
        *browser()->window()->GetColorProvider(), primary_highlight_color);
    ProfileThemeColors colors = {
        /*profile_highlight_color=*/primary_highlight_color,
        /*default_avatar_fill_color=*/avatar_colors.fill_color,
        /*default_avatar_stroke_color=*/avatar_colors.stroke_color};
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
        GetParam().intercepted_account_management_state ==
                ManagedAccountState::kEnterpriseAccount
            ? "intercepted.com"
            : kNoHostedDomainFound;
    if (GetParam().intercepted_account_management_state ==
        ManagedAccountState::kSupervisedAccount) {
      AccountCapabilitiesTestMutator mutator(&intercepted_account.capabilities);
      mutator.set_is_subject_to_parental_controls(true);
    }

    AccountInfo primary_account;
    primary_account.account_id = CoreAccountId::FromGaiaId("primary_ID");
    primary_account.given_name = "Tessa";
    primary_account.full_name = "Tessa Tester";
    primary_account.email = "tessa.tester@primary.com";
    primary_account.hosted_domain =
        GetParam().primary_account_management_state ==
                ManagedAccountState::kEnterpriseAccount
            ? "primary.com"
            : kNoHostedDomainFound;
    bool show_managed_disclaimer =
        (GetParam().intercepted_account_management_state ==
             ManagedAccountState::kEnterpriseAccount ||
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

IN_PROC_BROWSER_TEST_P(DiceWebSigninInterceptionBubblePixelTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(All,
                         DiceWebSigninInterceptionBubblePixelTest,
                         testing::ValuesIn(kTestParams),
                         &ParamToTestSuffix);
