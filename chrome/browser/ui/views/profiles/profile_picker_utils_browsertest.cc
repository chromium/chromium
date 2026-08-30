// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_utils.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_deref.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_callback_support.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "url/gurl.h"

namespace {

using ProfilePickerUtilsBrowserTest = InProcessBrowserTest;

struct PolicyTestParam {
  std::string_view key;
  std::string_view value;
  bool is_first_run_disabled = true;
};

const PolicyTestParam kPolicyTestParams[] = {
    {.key = policy::key::kSyncDisabled, .value = "true"},
    {.key = policy::key::kBrowserSignin, .value = "0"},
    {.key = policy::key::kBrowserSignin,
     .value = "1",
     .is_first_run_disabled = false},
#if !BUILDFLAG(IS_LINUX)
    {.key = policy::key::kBrowserSignin, .value = "2"},
#endif  // BUILDFLAG(IS_LINUX)
    {.key = policy::key::kPromotionalTabsEnabled, .value = "false"},
};

class ProfilePickerUtilsPolicyBrowserTest
    : public policy::PolicyTest,
      public testing::WithParamInterface<PolicyTestParam> {
 public:
  void SetPolicy(std::string_view key, std::string_view value) {
    policy::PolicyMap policy;
    policy.Set(
        std::string(key), policy::POLICY_LEVEL_MANDATORY,
        policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_PLATFORM,
        base::JSONReader::Read(value, base::JSON_PARSE_CHROMIUM_EXTENSIONS),
        /*external_data_fetcher=*/nullptr);

    base::RunLoop run_loop;
    policy::MockPolicyServiceObserver observer;
    EXPECT_CALL(observer, OnPolicyUpdated)
        .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

    policy::PolicyService& policy_service =
        CHECK_DEREF(g_browser_process->policy_service());
    policy_service.AddObserver(policy::POLICY_DOMAIN_CHROME, &observer);
    provider_.UpdateChromePolicy(policy);
    run_loop.Run();
    policy_service.RemoveObserver(policy::POLICY_DOMAIN_CHROME, &observer);
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ProfilePickerUtilsBrowserTest, OpenLearnMorePopup) {
  blink::mojom::WindowFeatures window_features;

  auto contents = content::WebContents::Create(
      content::WebContents::CreateParams(browser()->GetProfile()));
  content::WebContents* raw_contents = contents.get();

  ui_test_utils::BrowserCreatedObserver observer;

  OpenLearnMorePopup(browser()->GetProfile(), std::move(contents),
                     /*target_url=*/GURL(url::kAboutBlankURL), window_features);

  BrowserWindowInterface* popup_browser = observer.Wait();
  ASSERT_NE(popup_browser, nullptr);

  EXPECT_NE(popup_browser, browser());
  EXPECT_EQ(popup_browser->GetType(), BrowserWindowInterface::Type::TYPE_POPUP);
  EXPECT_EQ(popup_browser->GetProfile(), browser()->GetProfile());
  EXPECT_EQ(popup_browser->GetTabStripModel()->GetActiveWebContents(),
            raw_contents);
}

IN_PROC_BROWSER_TEST_F(ProfilePickerUtilsBrowserTest,
                       ComputeFirstRunSkipReasonProfileAlreadyExists) {
  auto* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->GetProfile());
  EXPECT_EQ(
      ComputeFirstRunSkipReason(*browser()->GetProfile(), *identity_manager),
      std::nullopt);

  signin::MakePrimaryAccountAvailable(identity_manager, "user@gmail.com",
                                      signin::ConsentLevel::kSignin);
  EXPECT_EQ(
      ComputeFirstRunSkipReason(*browser()->GetProfile(), *identity_manager),
      ProfilePicker::FirstRunFinishReason::kProfileAlreadySetUp);
}

IN_PROC_BROWSER_TEST_P(ProfilePickerUtilsPolicyBrowserTest,
                       ComputeFirstRunSkipReason) {
  signin_util::ResetForceSigninForTesting();

  SetPolicy(GetParam().key, GetParam().value);

  auto* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->GetProfile());
  std::optional<ProfilePicker::FirstRunFinishReason> expected_skip_reason;
  if (GetParam().is_first_run_disabled) {
    if (signin_util::IsForceSigninEnabled()) {
      expected_skip_reason = ProfilePicker::FirstRunFinishReason::kForceSignin;
    } else {
      expected_skip_reason =
          ProfilePicker::FirstRunFinishReason::kSkippedByPolicies;
    }
  }

  EXPECT_EQ(
      ComputeFirstRunSkipReason(*browser()->GetProfile(), *identity_manager),
      expected_skip_reason);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ProfilePickerUtilsPolicyBrowserTest,
    testing::ValuesIn(kPolicyTestParams),
    [](const testing::TestParamInfo<PolicyTestParam>& info) {
      return base::StrCat({info.param.key, info.param.value});
    });
