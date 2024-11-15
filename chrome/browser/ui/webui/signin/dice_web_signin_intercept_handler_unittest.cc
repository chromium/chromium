// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/dice_web_signin_intercept_handler.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/dice_web_signin_interceptor.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/supervised_user/core/common/features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/test_web_ui.h"
#include "google_apis/gaia/core_account_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

struct BubbleStrings {
  std::string header_text;
  std::string body_title;
  std::string body_text;
  std::string confirm_button_label;
  std::string cancel_button_label;
};

// Type of a function that generate strings we can use to validate expectations
// relative to a signin interception bubble.
// We need a generator function because resource strings need the test setup
// to be completed before we try to obtain them from IDs.
using ExpectedStringGenerator = base::RepeatingCallback<BubbleStrings()>;

struct TestParam {
  WebSigninInterceptor::SigninInterceptionType interception_type;
  policy::EnterpriseManagementAuthority management_authority;
  ExpectedStringGenerator expected_strings;
};

AccountInfo CreateAccount(std::string gaia_id,
                          std::string given_name,
                          std::string full_name,
                          std::string email,
                          std::string hosted_domain = kNoHostedDomainFound) {
  AccountInfo account_info;
  account_info.account_id = CoreAccountId::FromGaiaId(gaia_id);
  account_info.given_name = given_name;
  account_info.full_name = full_name;
  account_info.email = email;
  account_info.hosted_domain = hosted_domain;
  return account_info;
}

const AccountInfo primary_account = CreateAccount(
    /*gaia_id=*/"primary_ID",
    /*given_name=*/"Tessa",
    /*full_name=*/"Tessa Tester",
    /*email=*/"tessa.tester@primary.com",
    /*hosted_domain=*/kNoHostedDomainFound);

AccountInfo intercepted_account = CreateAccount(
    /*gaia_id=*/"intercepted_ID",
    /*given_name=*/"Sam",
    /*full_name=*/"Sam Sample",
    /*email=*/"sam.sample@intercepted.com",
    /*hosted_domain=*/kNoHostedDomainFound);

// Permutations of supported bubbles.
const TestParam kTestParams[] = {
    {
        WebSigninInterceptor::SigninInterceptionType::kMultiUser,
        policy::EnterpriseManagementAuthority::NONE,
        /*expected_strings=*/base::BindRepeating([] {
          return BubbleStrings{
              /*header_text=*/"",
              /*body_title=*/
              l10n_util::GetStringUTF8(
                  IDS_SIGNIN_DICE_WEB_INTERCEPT_CREATE_BUBBLE_TITLE),
              /*body_text=*/
              l10n_util::GetStringFUTF8(
                  IDS_SIGNIN_DICE_WEB_INTERCEPT_CONSUMER_BUBBLE_DESC,
                  base::UTF8ToUTF16(primary_account.given_name)),
              /*confirm_button_label=*/
              l10n_util::GetStringUTF8(
                  IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_NEW_PROFILE_BUTTON_LABEL),
              /*cancel_button_label=*/
              l10n_util::GetStringUTF8(
                  IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CANCEL_BUTTON_LABEL),
          };
        }),
    },
    {
        WebSigninInterceptor::SigninInterceptionType::kMultiUser,
        policy::EnterpriseManagementAuthority::CLOUD_DOMAIN,
        /*expected_strings=*/base::BindRepeating([] {
          return BubbleStrings{
              /*header_text=*/"",
              /*body_title=*/
              l10n_util::GetStringUTF8(
                  IDS_SIGNIN_DICE_WEB_INTERCEPT_CREATE_BUBBLE_TITLE),
              /*body_text=*/
              l10n_util::GetStringFUTF8(
                  IDS_SIGNIN_DICE_WEB_INTERCEPT_CONSUMER_BUBBLE_DESC_MANAGED_DEVICE,
                  base::UTF8ToUTF16(primary_account.given_name),
                  base::UTF8ToUTF16(intercepted_account.email)),
              /*confirm_button_label=*/
              l10n_util::GetStringUTF8(
                  IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_NEW_PROFILE_BUTTON_LABEL),
              /*cancel_button_label=*/
              l10n_util::GetStringUTF8(
                  IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CANCEL_BUTTON_LABEL),
          };
        }),
    },
    {
        WebSigninInterceptor::SigninInterceptionType::kEnterprise,
        policy::EnterpriseManagementAuthority::NONE,
        /*expected_strings=*/base::BindRepeating([] {
          return BubbleStrings{
              /*header_text=*/"",
              /*body_title=*/
              l10n_util::GetStringUTF8(
                  IDS_SIGNIN_DICE_WEB_INTERCEPT_CREATE_BUBBLE_TITLE),
              /*body_text=*/
              l10n_util::GetStringFUTF8(
                  IDS_SIGNIN_DICE_WEB_INTERCEPT_ENTERPRISE_BUBBLE_DESC,
                  base::UTF8ToUTF16(primary_account.email)),
              /*confirm_button_label=*/
              l10n_util::GetStringUTF8(
                  IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_NEW_PROFILE_BUTTON_LABEL),
              /*cancel_button_label=*/
              l10n_util::GetStringUTF8(
                  IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CANCEL_BUTTON_LABEL),
          };
        }),
    },
    {
        WebSigninInterceptor::SigninInterceptionType::kEnterprise,
        policy::EnterpriseManagementAuthority::CLOUD_DOMAIN,
        /*expected_strings=*/base::BindRepeating([] {
          return BubbleStrings{
              /*header_text=*/"",
              /*body_title=*/
              l10n_util::GetStringUTF8(
                  IDS_SIGNIN_DICE_WEB_INTERCEPT_CREATE_BUBBLE_TITLE),
              /*body_text=*/
              l10n_util::GetStringFUTF8(
                  IDS_SIGNIN_DICE_WEB_INTERCEPT_ENTERPRISE_BUBBLE_DESC_MANAGED_DEVICE,
                  base::UTF8ToUTF16(intercepted_account.email)),
              /*confirm_button_label=*/
              l10n_util::GetStringUTF8(
                  IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_NEW_PROFILE_BUTTON_LABEL),
              /*cancel_button_label=*/
              l10n_util::GetStringUTF8(
                  IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CANCEL_BUTTON_LABEL),
          };
        }),
    },
    {
        WebSigninInterceptor::SigninInterceptionType::kProfileSwitch,
        policy::EnterpriseManagementAuthority::NONE,
        /*expected_strings=*/base::BindRepeating([] {
          return BubbleStrings{
              /*header_text=*/intercepted_account.given_name,
              /*body_title=*/
              l10n_util::GetStringUTF8(
                  IDS_SIGNIN_DICE_WEB_INTERCEPT_SWITCH_BUBBLE_TITLE),
              /*body_text=*/
              l10n_util::GetStringUTF8(
                  IDS_SIGNIN_DICE_WEB_INTERCEPT_SWITCH_BUBBLE_DESC),
              /*confirm_button_label=*/
              l10n_util::GetStringUTF8(
                  IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CONFIRM_SWITCH_BUTTON_LABEL),
              /*cancel_button_label=*/
              l10n_util::GetStringUTF8(
                  IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CANCEL_SWITCH_BUTTON_LABEL),
          };
        }),
    },
};

}  // namespace

class DiceWebSigninInterceptHandlerTestBase : public testing::Test {
 public:
  DiceWebSigninInterceptHandlerTestBase()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

  base::Value::Dict GetInterceptionParameters() {
    Profile* profile = profile_manager_.CreateTestingProfile("Primary Profile");
    // Resetting the platform authority to NONE, as not all platforms have the
    // same value in browser tests. See https://crbug.com/1324377.
    policy::ScopedManagementServiceOverrideForTesting
        platform_management_authority_override(
            policy::ManagementServiceFactory::GetForPlatform(),
            policy::EnterpriseManagementAuthority::NONE);
    policy::ScopedManagementServiceOverrideForTesting
        profile_management_authority_override(
            policy::ManagementServiceFactory::GetForProfile(profile),
            management_authority());

    web_ui_.set_web_contents(web_contents_factory_.CreateWebContents(profile));

    DiceWebSigninInterceptHandler handler(
        {interception_type(), intercepted_account, primary_account},
        base::DoNothing(), base::DoNothing());
    handler.set_web_ui(&web_ui_);

    if (interception_type() ==
        WebSigninInterceptor::SigninInterceptionType::kChromeSignin) {
      return handler.GetInterceptionChromeSigninParametersValue();
    }

    return handler.GetInterceptionParametersValue();
  }

  // Returns the Management Authority that applies to the test case.
  virtual policy::EnterpriseManagementAuthority management_authority() = 0;
  // Returns the SigninInterceptionType that applies to the test case.
  virtual WebSigninInterceptor::SigninInterceptionType interception_type() = 0;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  content::TestWebContentsFactory web_contents_factory_;
  content::TestWebUI web_ui_;
};

class DiceWebSigninInterceptHandlerTest
    : public DiceWebSigninInterceptHandlerTestBase,
      public testing::WithParamInterface<TestParam> {
 public:
  DiceWebSigninInterceptHandlerTest() {
    feature_list_.InitWithFeatures(
        {}, {switches::kExplicitBrowserSigninUIOnDesktop});
  }

 protected:
  void ExpectStringsMatch(const base::Value::Dict& parameters,
                          const BubbleStrings& expected_strings) {
    EXPECT_EQ(*parameters.FindString("headerText"),
              expected_strings.header_text);
    EXPECT_EQ(*parameters.FindString("bodyTitle"), expected_strings.body_title);
    EXPECT_EQ(*parameters.FindString("bodyText"), expected_strings.body_text);
    EXPECT_EQ(*parameters.FindString("confirmButtonLabel"),
              expected_strings.confirm_button_label);
    EXPECT_EQ(*parameters.FindString("cancelButtonLabel"),
              expected_strings.cancel_button_label);
  }

  // DiceWebSigninInterceptHandlerTestBase override:
  policy::EnterpriseManagementAuthority management_authority() override {
    return GetParam().management_authority;
  }
  WebSigninInterceptor::SigninInterceptionType interception_type() override {
    return GetParam().interception_type;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(DiceWebSigninInterceptHandlerTest, CheckStrings) {
  base::Value::Dict parameters = GetInterceptionParameters();

  if (GetParam().interception_type !=
      WebSigninInterceptor::SigninInterceptionType::kProfileSwitch) {
    EXPECT_TRUE(*parameters.FindBool("useV2Design"));
  }
  ExpectStringsMatch(parameters, GetParam().expected_strings.Run());
}

INSTANTIATE_TEST_SUITE_P(All,
                         DiceWebSigninInterceptHandlerTest,
                         testing::ValuesIn(kTestParams));

// Tests the parameters of the Chrome Sign In interception bubble.
class DiceWebSigninInterceptHandlerChromeSigninInterceptionTest
    : public DiceWebSigninInterceptHandlerTestBase,
      public testing::WithParamInterface<
          std::tuple</*is_supervised_user=*/signin::Tribool,
                     /*is_supervised_users_ui_feature_enabled=*/bool>> {
 public:
  DiceWebSigninInterceptHandlerChromeSigninInterceptionTest() {
    CHECK(interception_type() ==
          WebSigninInterceptor::SigninInterceptionType::kChromeSignin);
    if (IsSupervisedUsersUiFeatureEnabled()) {
      feature_list_.InitWithFeatures(
          {supervised_user::kCustomProfileStringsForSupervisedUsers}, {});
    } else {
      feature_list_.InitWithFeatures(
          {}, {supervised_user::kCustomProfileStringsForSupervisedUsers});
    }

    AccountCapabilitiesTestMutator mutator(&intercepted_account.capabilities);
    switch (IsSupervisedUser()) {
      case signin::Tribool::kTrue:
        mutator.set_is_subject_to_parental_controls(true);
        break;
      case signin::Tribool::kFalse:
        mutator.set_is_subject_to_parental_controls(false);
        break;
      default:
        break;
    }
  }

  void ExpectChromeSignInStringsMatch(const base::Value::Dict& parameters) {
    std::string title = l10n_util::GetStringUTF8(
        IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CHROME_SIGNIN_TITLE);
    std::string subtitle = l10n_util::GetStringUTF8(
        IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CHROME_SIGNIN_SUBTITLE);
    if (IsSupervisedUsersUiFeatureEnabled() &&
        IsSupervisedUser() == signin::Tribool::kTrue) {
      title = l10n_util::GetStringUTF8(
          IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CHROME_SIGNIN_TITLE_SUPERVISED);
      subtitle = l10n_util::GetStringUTF8(
          IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CHROME_SIGNIN_SUBTITLE_SUPERVISED);
    }

    EXPECT_EQ(*parameters.FindString("title"), title);
    EXPECT_EQ(*parameters.FindString("subtitle"), subtitle);
  }

 protected:
  // DiceWebSigninInterceptHandlerTestBase override:
  policy::EnterpriseManagementAuthority management_authority() override {
    return policy::EnterpriseManagementAuthority::NONE;
  }
  WebSigninInterceptor::SigninInterceptionType interception_type() override {
    return WebSigninInterceptor::SigninInterceptionType::kChromeSignin;
  }

 private:
  bool IsSupervisedUsersUiFeatureEnabled() { return std::get<1>(GetParam()); }

  signin::Tribool IsSupervisedUser() { return std::get<0>(GetParam()); }

  base::test::ScopedFeatureList feature_list_;
};

TEST_P(DiceWebSigninInterceptHandlerChromeSigninInterceptionTest,
       CheckStrings) {
  base::Value::Dict parameters = GetInterceptionParameters();
  ExpectChromeSignInStringsMatch(parameters);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DiceWebSigninInterceptHandlerChromeSigninInterceptionTest,
    testing::Combine(
        /*is_supervised_user=*/testing::Values(signin::Tribool::kTrue,
                                               signin::Tribool::kFalse,
                                               signin::Tribool::kUnknown),
        /*is_supervised_users_ui_feature_enabled=*/testing::Bool()));
