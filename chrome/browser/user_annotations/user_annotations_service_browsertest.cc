// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_annotations/user_annotations_service.h"

#include <optional>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/user_annotations/user_annotations_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill_ai/core/browser/autofill_ai_features.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/user_annotations/user_annotations_types.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/guest_session_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#endif

namespace user_annotations {

class UserAnnotationsServiceDisabledBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitAndDisableFeature(autofill_ai::kAutofillAi);
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(UserAnnotationsServiceDisabledBrowserTest,
                       ServiceNotCreatedWhenFeatureDisabled) {
  ASSERT_FALSE(
      UserAnnotationsServiceFactory::GetForProfile(browser()->profile()));
}

class UserAnnotationsServiceKioskModeBrowserTest : public InProcessBrowserTest {
 public:
  UserAnnotationsServiceKioskModeBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(autofill_ai::kAutofillAi);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(::switches::kKioskMode);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(UserAnnotationsServiceKioskModeBrowserTest,
                       DisabledInKioskMode) {
  EXPECT_EQ(nullptr,
            UserAnnotationsServiceFactory::GetForProfile(browser()->profile()));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
class UserAnnotationsServiceEphemeralProfileBrowserTest
    : public MixinBasedInProcessBrowserTest {
 public:
  UserAnnotationsServiceEphemeralProfileBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(autofill_ai::kAutofillAi);
  }

 private:
  ash::GuestSessionMixin guest_session_{&mixin_host_};

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(UserAnnotationsServiceEphemeralProfileBrowserTest,
                       EphemeralProfileDoesNotInstantiateService) {
  EXPECT_EQ(nullptr,
            UserAnnotationsServiceFactory::GetForProfile(browser()->profile()));
}
#endif

class UserAnnotationsServiceBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    InitializeFeatureList();
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    InProcessBrowserTest::SetUpOnMainThread();

    browser()->profile()->GetPrefs()->SetBoolean(
        autofill::prefs::kAutofillPredictionImprovementsEnabled, true);

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "components/test/data/autofill");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&UserAnnotationsServiceBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void EnableSignin() {
    auto account_info =
        identity_test_env_adaptor_->identity_test_env()
            ->MakePrimaryAccountAvailable("user@gmail.com",
                                          signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_use_model_execution_features(true);
    identity_test_env_adaptor_->identity_test_env()
        ->UpdateAccountInfoForAccount(account_info);
    identity_test_env_adaptor_->identity_test_env()
        ->SetAutomaticIssueOfAccessTokens(true);
  }

  testing::AssertionResult SubmitForm(content::RenderFrameHost* rfh) {
    return content::ExecJs(rfh, R"(document.forms[0].submit();)");
  }

  testing::AssertionResult FillForm(content::RenderFrameHost* rfh) {
    return content::ExecJs(rfh, R"(
        document.getElementsByName("name")[0].value="John Doe";
        document.getElementsByName("address")[0].value="123 Main Street";
        document.getElementsByName("city")[0].value="Knightdale";
        document.getElementsByName("state")[0].selectedIndex=3;
        document.getElementsByName("zip")[0].value="27545";
        document.getElementsByName("country")[0].value="United States";
        document.getElementsByName("email")[0].value="jd@example.com";
        document.getElementsByName("phone")[0].value="919-555-5555";
        )");
  }

 protected:
  UserAnnotationsService* service() {
    return UserAnnotationsServiceFactory::GetForProfile(browser()->profile());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  virtual void InitializeFeatureList() {
    feature_list_.InitWithFeatures({autofill_ai::kAutofillAi}, {});
  }

  base::test::ScopedFeatureList feature_list_;

 private:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  // Identity test support.
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(UserAnnotationsServiceBrowserTest, ServiceFactoryWorks) {
  EXPECT_TRUE(service());
}

IN_PROC_BROWSER_TEST_F(UserAnnotationsServiceBrowserTest,
                       ServiceNotCreatedForIncognito) {
  Browser* otr_browser = CreateIncognitoBrowser(browser()->profile());
  EXPECT_EQ(nullptr, UserAnnotationsServiceFactory::GetForProfile(
                         otr_browser->profile()));
}

// TODO(crbug.com/367201367):  Re-enable once flakiness is resolved for Windows
// ASAN. Also flaky on Mac.
IN_PROC_BROWSER_TEST_F(UserAnnotationsServiceBrowserTest,
                       DISABLED_FormSubmissionFlow) {
  EnableSignin();

  base::HistogramTester histogram_tester;

  GURL url(
      embedded_test_server()->GetURL("a.com", "/autofill_address_form.html"));
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->AddHintForTesting(url, optimization_guide::proto::FORMS_ANNOTATIONS,
                          std::nullopt);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_TRUE(FillForm(web_contents()->GetPrimaryMainFrame()));
  ASSERT_TRUE(SubmitForm(web_contents()->GetPrimaryMainFrame()));

  EXPECT_EQ(
      1, optimization_guide::RetryForHistogramUntilCountReached(
             &histogram_tester, "UserAnnotations.AddFormSubmissionResult", 1));
  histogram_tester.ExpectTotalCount("UserAnnotations.AddFormSubmissionResult",
                                    1);
}

IN_PROC_BROWSER_TEST_F(UserAnnotationsServiceBrowserTest, NotOnAllowlist) {
  EnableSignin();

  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("notallowed.com",
                                          "/autofill_address_form.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_TRUE(FillForm(web_contents()->GetPrimaryMainFrame()));
  ASSERT_TRUE(SubmitForm(web_contents()->GetPrimaryMainFrame()));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount("UserAnnotations.AddFormSubmissionResult",
                                    0);
}

// TODO: b/361692317 - Delete below once optimization guide populates list.

class UserAnnotationsServiceExplicitAllowlistBrowserTest
    : public UserAnnotationsServiceBrowserTest {
 protected:
  void InitializeFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{autofill_ai::kAutofillAi,
          {{"allowed_hosts_for_form_submissions", "allowed.com"}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(UserAnnotationsServiceExplicitAllowlistBrowserTest,
                       NotOnAllowlist) {
  EnableSignin();

  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("notallowed.com",
                                          "/autofill_address_form.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_TRUE(FillForm(web_contents()->GetPrimaryMainFrame()));
  ASSERT_TRUE(SubmitForm(web_contents()->GetPrimaryMainFrame()));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount("UserAnnotations.AddFormSubmissionResult",
                                    0);
}

// TODO(crbug.com/367201367):  Re-enable once flakiness is resolved for Windows
// ASAN. Also flaky on Mac.
IN_PROC_BROWSER_TEST_F(UserAnnotationsServiceExplicitAllowlistBrowserTest,
                       DISABLED_OnAllowlist) {
  EnableSignin();

  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("allowed.com",
                                          "/autofill_address_form.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_TRUE(FillForm(web_contents()->GetPrimaryMainFrame()));
  ASSERT_TRUE(SubmitForm(web_contents()->GetPrimaryMainFrame()));

  EXPECT_EQ(
      1, optimization_guide::RetryForHistogramUntilCountReached(
             &histogram_tester, "UserAnnotations.AddFormSubmissionResult", 1));
  histogram_tester.ExpectTotalCount("UserAnnotations.AddFormSubmissionResult",
                                    1);
}

}  // namespace user_annotations
