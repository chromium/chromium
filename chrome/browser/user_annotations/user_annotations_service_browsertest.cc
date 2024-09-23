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
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/user_annotations/user_annotations_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_features.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/user_annotations/user_annotations_features.h"
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
    feature_list_.InitAndDisableFeature(kUserAnnotations);
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
    scoped_feature_list_.InitAndEnableFeature(kUserAnnotations);
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
    scoped_feature_list_.InitAndEnableFeature(kUserAnnotations);
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

class UserAnnotationsServiceBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    InitializeFeatureList();
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    InProcessBrowserTest::SetUpOnMainThread();

    embedded_test_server()->ServeFilesFromSourceDirectory(
        "components/test/data/autofill");
    ASSERT_TRUE(embedded_test_server()->Start());
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

  bool ShouldObserveFormSubmissions() { return GetParam(); }

 protected:
  UserAnnotationsService* service() {
    return UserAnnotationsServiceFactory::GetForProfile(browser()->profile());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  virtual void InitializeFeatureList() {
    std::vector<base::test::FeatureRef> enabled_features = {
        kUserAnnotations,
        autofill_prediction_improvements::kAutofillPredictionImprovements};
    std::vector<base::test::FeatureRef> disabled_features = {
        kUserAnnotationsObserveFormSubmissions};
    if (ShouldObserveFormSubmissions()) {
      enabled_features.emplace_back(kUserAnnotationsObserveFormSubmissions);
      disabled_features.clear();
    }
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(UserAnnotationsServiceBrowserTest, ServiceFactoryWorks) {
  EXPECT_TRUE(service());
}

IN_PROC_BROWSER_TEST_P(UserAnnotationsServiceBrowserTest,
                       ServiceNotCreatedForIncognito) {
  Browser* otr_browser = CreateIncognitoBrowser(browser()->profile());
  EXPECT_EQ(nullptr, UserAnnotationsServiceFactory::GetForProfile(
                         otr_browser->profile()));
}

// Flakily times out b/366323026
IN_PROC_BROWSER_TEST_P(UserAnnotationsServiceBrowserTest,
                       DISABLED_FormSubmissionFlow) {
  base::HistogramTester histogram_tester;

  GURL url(
      embedded_test_server()->GetURL("a.com", "/autofill_address_form.html"));
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->AddHintForTesting(url, optimization_guide::proto::FORMS_ANNOTATIONS,
                          std::nullopt);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_TRUE(FillForm(web_contents()->GetPrimaryMainFrame()));
  ASSERT_TRUE(SubmitForm(web_contents()->GetPrimaryMainFrame()));

  EXPECT_EQ(1, optimization_guide::RetryForHistogramUntilCountReached(
                   &histogram_tester,
                   "OptimizationGuide.ModelExecutionFetcher.RequestStatus."
                   "FormsAnnotations",
                   1));
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutionFetcher.RequestStatus.FormsAnnotations",
      1);
}

IN_PROC_BROWSER_TEST_P(UserAnnotationsServiceBrowserTest, NotOnAllowlist) {
  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("notallowed.com",
                                          "/autofill_address_form.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_TRUE(FillForm(web_contents()->GetPrimaryMainFrame()));
  ASSERT_TRUE(SubmitForm(web_contents()->GetPrimaryMainFrame()));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutionFetcher.RequestStatus.FormsAnnotations",
      0);
}

INSTANTIATE_TEST_SUITE_P(All,
                         UserAnnotationsServiceBrowserTest,
                         ::testing::Bool());

// TODO: b/361692317 - Delete below once optimization guide populates list.

class UserAnnotationsServiceExplicitAllowlistBrowserTest
    : public UserAnnotationsServiceBrowserTest {
 protected:
  void InitializeFeatureList() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features_and_params = {
        {kUserAnnotations,
         {{"allowed_hosts_for_form_submissions", "allowed.com"}}},
        {autofill_prediction_improvements::kAutofillPredictionImprovements,
         {}}};
    std::vector<base::test::FeatureRef> disabled_features = {
        kUserAnnotationsObserveFormSubmissions};
    if (ShouldObserveFormSubmissions()) {
      enabled_features_and_params.emplace_back(base::test::FeatureRefAndParams{
          kUserAnnotationsObserveFormSubmissions, {}});
      disabled_features.clear();
    }
    feature_list_.InitWithFeaturesAndParameters(enabled_features_and_params,
                                                disabled_features);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(UserAnnotationsServiceExplicitAllowlistBrowserTest,
                       NotOnAllowlist) {
  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("notallowed.com",
                                          "/autofill_address_form.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_TRUE(FillForm(web_contents()->GetPrimaryMainFrame()));
  ASSERT_TRUE(SubmitForm(web_contents()->GetPrimaryMainFrame()));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutionFetcher.RequestStatus.FormsAnnotations",
      0);
}

IN_PROC_BROWSER_TEST_P(UserAnnotationsServiceExplicitAllowlistBrowserTest,
                       OnAllowlist) {
  if (!GetParam()) {
    // TODO(b/367201367): Test is flaky in this case. Re-enable when fixed.
    return;
  }

  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("allowed.com",
                                          "/autofill_address_form.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_TRUE(FillForm(web_contents()->GetPrimaryMainFrame()));
  ASSERT_TRUE(SubmitForm(web_contents()->GetPrimaryMainFrame()));

  EXPECT_EQ(1, optimization_guide::RetryForHistogramUntilCountReached(
                   &histogram_tester,
                   "OptimizationGuide.ModelExecutionFetcher.RequestStatus."
                   "FormsAnnotations",
                   1));
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutionFetcher.RequestStatus.FormsAnnotations",
      1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         UserAnnotationsServiceExplicitAllowlistBrowserTest,
                         ::testing::Bool());

}  // namespace user_annotations
