// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/webshare/share_service_impl.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/safe_browsing/content/common/file_type_policies_test_util.h"
#include "components/safe_browsing/core/browser/db/fake_database_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chrome/browser/webshare/chromeos/sharesheet_client.h"
#include "chromeos/components/sharesheet/constants.h"
#endif
#if BUILDFLAG(IS_WIN)
#include "chrome/browser/webshare/win/scoped_share_operation_fake_components.h"
#endif
#if BUILDFLAG(IS_MAC)
#include "chrome/browser/webshare/mac/sharing_service_operation.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"
#endif

class ShareServiceBrowserTest : public InProcessBrowserTest {
 public:
  ShareServiceBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kWebShare);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
#if BUILDFLAG(IS_CHROMEOS)
    webshare::SharesheetClient::SetSharesheetCallbackForTesting(
        base::BindRepeating(&ShareServiceBrowserTest::AcceptShareRequest));
#endif
#if BUILDFLAG(IS_WIN)
    ASSERT_NO_FATAL_FAILURE(scoped_fake_components_.SetUp());
#endif
#if BUILDFLAG(IS_MAC)
    webshare::SharingServiceOperation::SetSharePickerCallbackForTesting(
        base::BindRepeating(&ShareServiceBrowserTest::AcceptShareRequest));
#endif
  }

#if BUILDFLAG(IS_CHROMEOS)
  static void AcceptShareRequest(
      content::WebContents* web_contents,
      const std::vector<base::FilePath>& file_paths,
      const std::vector<std::string>& content_types,
      const std::vector<uint64_t>& file_sizes,
      const std::string& text,
      const std::string& title,
      sharesheet::DeliveredCallback delivered_callback) {
    std::move(delivered_callback).Run(sharesheet::SharesheetResult::kSuccess);
  }
#endif

#if BUILDFLAG(IS_MAC)
  static void AcceptShareRequest(
      content::WebContents* web_contents,
      const std::vector<base::FilePath>& file_paths,
      const std::string& text,
      const std::string& title,
      const GURL& url,
      blink::mojom::ShareService::ShareCallback close_callback) {
    std::move(close_callback).Run(blink::mojom::ShareError::OK);
  }
#endif

 private:
  base::test::ScopedFeatureList feature_list_;
#if BUILDFLAG(IS_WIN)
  webshare::ScopedShareOperationFakeComponents scoped_fake_components_;
#endif
};

IN_PROC_BROWSER_TEST_F(ShareServiceBrowserTest, Text) {
  const int kRepeats = 4;

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/webshare/index.html")));

  content::WebContents* const contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  const std::string script = "share_text('hello')";
  for (int index = 0; index < kRepeats; ++index) {
    const content::EvalJsResult result = content::EvalJs(contents, script);
    EXPECT_EQ("share succeeded", result);
  }

  histogram_tester.ExpectBucketCount(kWebShareApiCountMetric,
                                     WebShareMethod::kShare, kRepeats);
}

class SafeBrowsingShareServiceBrowserTest : public ShareServiceBrowserTest {
 public:
  SafeBrowsingShareServiceBrowserTest()
      : safe_browsing_factory_(
            std::make_unique<safe_browsing::TestSafeBrowsingServiceFactory>()) {
  }

 protected:
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    fake_safe_browsing_database_manager_ =
        base::MakeRefCounted<safe_browsing::FakeSafeBrowsingDatabaseManager>(
            content::GetUIThreadTaskRunner({}));
    safe_browsing_factory_->SetTestDatabaseManager(
        fake_safe_browsing_database_manager_.get());
    safe_browsing::SafeBrowsingService::RegisterFactory(
        safe_browsing_factory_.get());
    ShareServiceBrowserTest::CreatedBrowserMainParts(browser_main_parts);
  }

  void AddDangerousUrl(const GURL& dangerous_url) {
    fake_safe_browsing_database_manager_->AddDangerousUrl(
        dangerous_url,
        safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_BINARY_MALWARE);
  }

  void TearDown() override {
    ShareServiceBrowserTest::TearDown();
    safe_browsing::SafeBrowsingService::RegisterFactory(nullptr);
  }

 private:
  scoped_refptr<safe_browsing::FakeSafeBrowsingDatabaseManager>
      fake_safe_browsing_database_manager_;
  std::unique_ptr<safe_browsing::TestSafeBrowsingServiceFactory>
      safe_browsing_factory_;
};

IN_PROC_BROWSER_TEST_F(SafeBrowsingShareServiceBrowserTest,
                       PortableDocumentFile) {
  safe_browsing::FileTypePoliciesTestOverlay policies;
  std::unique_ptr<safe_browsing::DownloadFileTypeConfig> file_type_config =
      std::make_unique<safe_browsing::DownloadFileTypeConfig>();
  auto* file_type = file_type_config->mutable_default_file_type();
  file_type->set_uma_value(-1);
  file_type->set_ping_setting(safe_browsing::DownloadFileType::FULL_PING);
  auto* platform_settings = file_type->add_platform_settings();
  platform_settings->set_danger_level(
      safe_browsing::DownloadFileType::NOT_DANGEROUS);
  platform_settings->set_auto_open_hint(
      safe_browsing::DownloadFileType::ALLOW_AUTO_OPEN);
  policies.SwapConfig(file_type_config);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/webshare/index.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* const contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ("share succeeded", content::EvalJs(contents, "share_pdf_file()"));

  AddDangerousUrl(url);
  EXPECT_EQ("share failed: NotAllowedError: Permission denied",
            content::EvalJs(contents, "share_pdf_file()"));
}

class ShareServicePrerenderBrowserTest : public ShareServiceBrowserTest {
 public:
  ShareServicePrerenderBrowserTest()
      : prerender_helper_(
            base::BindRepeating(&ShareServicePrerenderBrowserTest::web_contents,
                                base::Unretained(this))) {}
  ~ShareServicePrerenderBrowserTest() override = default;

 protected:
  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(ShareServicePrerenderBrowserTest, Text) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));

  content::WebContents* const contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Start a prerender.
  const GURL kPrerenderUrl =
      embedded_test_server()->GetURL("/webshare/index.html");
  const content::FrameTreeNodeId kPrerenderHostId =
      prerender_helper_.AddPrerender((kPrerenderUrl));
  ASSERT_EQ(prerender_helper_.GetHostForUrl(kPrerenderUrl), kPrerenderHostId);

  content::RenderFrameHost* prerender_rfh =
      prerender_helper_.GetPrerenderedMainFrameHost(kPrerenderHostId);
  EXPECT_EQ(prerender_rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kPrerendering);
  const std::string script = "share_text('hello')";
  const content::EvalJsResult prerendered_result =
      content::EvalJs(prerender_rfh, script);
  EXPECT_EQ(
      "share failed: NotAllowedError: Failed to execute 'share' on "
      "'Navigator': Must be handling a user gesture to perform a share "
      "request.",
      prerendered_result);
  histogram_tester.ExpectBucketCount(kWebShareApiCountMetric,
                                     WebShareMethod::kShare, 0);

  // Activate the prerendered page.
  prerender_helper_.NavigatePrimaryPage(kPrerenderUrl);
  EXPECT_EQ(prerender_rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kActive);
  ASSERT_EQ(kPrerenderUrl, contents->GetLastCommittedURL());
  const content::EvalJsResult activated_result =
      content::EvalJs(prerender_rfh, script);
  EXPECT_EQ("share succeeded", activated_result);
  histogram_tester.ExpectBucketCount(kWebShareApiCountMetric,
                                     WebShareMethod::kShare, 1);
}
