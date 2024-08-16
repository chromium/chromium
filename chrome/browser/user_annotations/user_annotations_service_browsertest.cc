// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_annotations/user_annotations_service.h"

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/user_annotations/user_annotations_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
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

class UserAnnotationsServiceBrowserTest : public InProcessBrowserTest {
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

 protected:
  UserAnnotationsService* service() {
    return UserAnnotationsServiceFactory::GetForProfile(browser()->profile());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  virtual void InitializeFeatureList() {
    feature_list_.InitAndEnableFeature(kUserAnnotations);
  }

  base::test::ScopedFeatureList feature_list_;
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

IN_PROC_BROWSER_TEST_F(UserAnnotationsServiceBrowserTest, FormSubmissionFlow) {
  base::HistogramTester histogram_tester;

  GURL url(
      embedded_test_server()->GetURL("a.com", "/autofill_address_form.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_TRUE(SubmitForm(web_contents()->GetPrimaryMainFrame()));

  EXPECT_EQ(1,
            optimization_guide::RetryForHistogramUntilCountReached(
                &histogram_tester, "UserAnnotations.DidAddFormSubmission", 1));
  histogram_tester.ExpectUniqueSample("UserAnnotations.DidAddFormSubmission",
                                      true, 1);

  base::test::TestFuture<
      std::vector<optimization_guide::proto::UserAnnotationsEntry>>
      test_future;
  service()->RetrieveAllEntries(test_future.GetCallback());

  auto entries = test_future.Take();
  EXPECT_FALSE(entries.empty());
}

class UserAnnotationsServiceExplicitAllowlistBrowserTest
    : public UserAnnotationsServiceBrowserTest {
 protected:
  void InitializeFeatureList() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        kUserAnnotations,
        {{"allowed_hosts_for_form_submissions", "allowed.com"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(UserAnnotationsServiceExplicitAllowlistBrowserTest,
                       NotOnAllowlist) {
  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("notallowed.com",
                                          "/autofill_address_form.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_TRUE(SubmitForm(web_contents()->GetPrimaryMainFrame()));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount("UserAnnotations.DidAddFormSubmission", 0);

  base::test::TestFuture<
      std::vector<optimization_guide::proto::UserAnnotationsEntry>>
      test_future;
  service()->RetrieveAllEntries(test_future.GetCallback());

  auto entries = test_future.Take();
  EXPECT_TRUE(entries.empty());
}

IN_PROC_BROWSER_TEST_F(UserAnnotationsServiceExplicitAllowlistBrowserTest,
                       OnAllowlist) {
  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("allowed.com",
                                          "/autofill_address_form.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_TRUE(SubmitForm(web_contents()->GetPrimaryMainFrame()));

  EXPECT_EQ(1,
            optimization_guide::RetryForHistogramUntilCountReached(
                &histogram_tester, "UserAnnotations.DidAddFormSubmission", 1));
  histogram_tester.ExpectUniqueSample("UserAnnotations.DidAddFormSubmission",
                                      true, 1);

  base::test::TestFuture<
      std::vector<optimization_guide::proto::UserAnnotationsEntry>>
      test_future;
  service()->RetrieveAllEntries(test_future.GetCallback());

  auto entries = test_future.Take();
  EXPECT_FALSE(entries.empty());
}

}  // namespace user_annotations
