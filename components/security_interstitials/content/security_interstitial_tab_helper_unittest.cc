// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/security_interstitial_tab_helper.h"

#include <memory>

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/time/time.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/net_errors.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "url/gurl.h"

namespace security_interstitials {

const char kTestSslMetricsName[] = "test_blocking_page";

std::unique_ptr<security_interstitials::MetricsHelper> CreateTestMetricsHelper(
    content::WebContents* web_contents) {
  MetricsHelper::ReportDetails report_details;
  report_details.metric_prefix = kTestSslMetricsName;
  return std::make_unique<security_interstitials::MetricsHelper>(
      GURL(), report_details, nullptr);
}

class TestInterstitialPage : public SecurityInterstitialPage {
 public:
  // |*destroyed_tracker| is set to true in the destructor.
  TestInterstitialPage(content::WebContents* web_contents,
                       const GURL& request_url,
                       bool* destroyed_tracker)
      : SecurityInterstitialPage(
            web_contents,
            request_url,
            std::make_unique<SecurityInterstitialControllerClient>(
                web_contents,
                CreateTestMetricsHelper(web_contents),
                nullptr,
                base::i18n::GetConfiguredLocale(),
                GURL())),
        destroyed_tracker_(destroyed_tracker) {}

  ~TestInterstitialPage() override { *destroyed_tracker_ = true; }

  void OnInterstitialClosing() override {}

 protected:
  bool ShouldCreateNewNavigation() const override { return false; }

  void PopulateInterstitialStrings(
      base::DictionaryValue* load_time_data) override {}

 private:
  bool* destroyed_tracker_;
};

class SecurityInterstitialTabHelperTest
    : public content::RenderViewHostTestHarness {
 protected:
  std::unique_ptr<content::NavigationHandle> CreateHandle(
      bool committed,
      bool is_same_document) {
    std::unique_ptr<content::MockNavigationHandle> handle =
        std::make_unique<content::MockNavigationHandle>(GURL(), main_rfh());
    handle->set_has_committed(committed);
    handle->set_is_same_document(is_same_document);
    return handle;
  }

  // The lifetime of the blocking page is managed by the
  // SecurityInterstitialTabHelper for the test's web_contents.
  // |destroyed_tracker| will be set to true when the corresponding blocking
  // page is destroyed.
  void CreateAssociatedBlockingPage(content::NavigationHandle* handle,
                                    bool* destroyed_tracker) {
    SecurityInterstitialTabHelper::AssociateBlockingPage(
        web_contents(), handle->GetNavigationId(),
        std::make_unique<TestInterstitialPage>(web_contents(), GURL(),
                                               destroyed_tracker));
  }
};

// Tests that the helper properly handles the lifetime of a single blocking
// page, interleaved with other navigations.
TEST_F(SecurityInterstitialTabHelperTest, SingleBlockingPage) {
  std::unique_ptr<content::NavigationHandle> blocking_page_handle =
      CreateHandle(true, false);
  bool blocking_page_destroyed = false;
  CreateAssociatedBlockingPage(blocking_page_handle.get(),
                               &blocking_page_destroyed);
  SecurityInterstitialTabHelper* helper =
      SecurityInterstitialTabHelper::FromWebContents(web_contents());

  // Test that a same-document navigation doesn't destroy the blocking page if
  // its navigation hasn't committed yet.
  std::unique_ptr<content::NavigationHandle> same_document_handle =
      CreateHandle(true, true);
  helper->DidFinishNavigation(same_document_handle.get());
  EXPECT_FALSE(blocking_page_destroyed);

  // Test that a committed (non-same-document) navigation doesn't destroy the
  // blocking page if its navigation hasn't committed yet.
  std::unique_ptr<content::NavigationHandle> committed_handle1 =
      CreateHandle(true, false);
  helper->DidFinishNavigation(committed_handle1.get());
  EXPECT_FALSE(blocking_page_destroyed);

  // Simulate comitting the interstitial.
  helper->DidFinishNavigation(blocking_page_handle.get());
  EXPECT_FALSE(blocking_page_destroyed);

  // Test that a subsequent committed navigation releases the blocking page
  // stored for the currently committed navigation.
  std::unique_ptr<content::NavigationHandle> committed_handle2 =
      CreateHandle(true, false);
  helper->DidFinishNavigation(committed_handle2.get());
  EXPECT_TRUE(blocking_page_destroyed);
}

// Tests that the helper properly handles the lifetime of multiple blocking
// pages, committed in a different order than they are created.
TEST_F(SecurityInterstitialTabHelperTest, DISABLED_MultipleBlockingPages) {
  // Simulate associating the first interstitial.
  std::unique_ptr<content::NavigationHandle> handle1 =
      CreateHandle(true, false);
  bool blocking_page1_destroyed = false;
  CreateAssociatedBlockingPage(handle1.get(), &blocking_page1_destroyed);

  // We can directly retrieve the helper for testing once
  // CreateAssociatedBlockingPage() was called.
  SecurityInterstitialTabHelper* helper =
      SecurityInterstitialTabHelper::FromWebContents(web_contents());

  // Simulate commiting the first interstitial.
  helper->DidFinishNavigation(handle1.get());
  EXPECT_FALSE(blocking_page1_destroyed);

  // Associate the second interstitial.
  std::unique_ptr<content::NavigationHandle> handle2 =
      CreateHandle(true, false);
  bool blocking_page2_destroyed = false;
  CreateAssociatedBlockingPage(handle2.get(), &blocking_page2_destroyed);
  EXPECT_FALSE(blocking_page1_destroyed);
  EXPECT_FALSE(blocking_page2_destroyed);

  // Associate the third interstitial.
  std::unique_ptr<content::NavigationHandle> handle3 =
      CreateHandle(true, false);
  bool blocking_page3_destroyed = false;
  CreateAssociatedBlockingPage(handle3.get(), &blocking_page3_destroyed);
  EXPECT_FALSE(blocking_page1_destroyed);
  EXPECT_FALSE(blocking_page2_destroyed);
  EXPECT_FALSE(blocking_page3_destroyed);

  // Simulate commiting the third interstitial.
  helper->DidFinishNavigation(handle3.get());
  EXPECT_TRUE(blocking_page1_destroyed);
  EXPECT_FALSE(blocking_page2_destroyed);
  EXPECT_FALSE(blocking_page3_destroyed);

  // Simulate commiting the second interstitial.
  helper->DidFinishNavigation(handle2.get());
  EXPECT_TRUE(blocking_page1_destroyed);
  EXPECT_FALSE(blocking_page2_destroyed);
  EXPECT_TRUE(blocking_page3_destroyed);

  // Test that a subsequent committed navigation releases the last blocking
  // page.
  std::unique_ptr<content::NavigationHandle> committed_handle4 =
      CreateHandle(true, false);
  helper->DidFinishNavigation(committed_handle4.get());
  EXPECT_TRUE(blocking_page2_destroyed);
}

// Tests that the helper properly handles a navigation that finishes without
// committing.
TEST_F(SecurityInterstitialTabHelperTest, NavigationDoesNotCommit) {
  std::unique_ptr<content::NavigationHandle> committed_handle =
      CreateHandle(true, false);
  bool committed_blocking_page_destroyed = false;
  CreateAssociatedBlockingPage(committed_handle.get(),
                               &committed_blocking_page_destroyed);
  SecurityInterstitialTabHelper* helper =
      SecurityInterstitialTabHelper::FromWebContents(web_contents());
  helper->DidFinishNavigation(committed_handle.get());
  EXPECT_FALSE(committed_blocking_page_destroyed);

  // Simulate a navigation that does not commit.
  std::unique_ptr<content::NavigationHandle> non_committed_handle =
      CreateHandle(false, false);
  bool non_committed_blocking_page_destroyed = false;
  CreateAssociatedBlockingPage(non_committed_handle.get(),
                               &non_committed_blocking_page_destroyed);
  helper->DidFinishNavigation(non_committed_handle.get());

  // The blocking page for the non-committed navigation should have been cleaned
  // up, but the one for the previous committed navigation should still be
  // around.
  EXPECT_TRUE(non_committed_blocking_page_destroyed);
  EXPECT_FALSE(committed_blocking_page_destroyed);

  // When a navigation does commit, the previous one should be cleaned up.
  std::unique_ptr<content::NavigationHandle> next_committed_handle =
      CreateHandle(true, false);
  helper->DidFinishNavigation(next_committed_handle.get());
  EXPECT_TRUE(committed_blocking_page_destroyed);
}

}  // namespace security_interstitials
