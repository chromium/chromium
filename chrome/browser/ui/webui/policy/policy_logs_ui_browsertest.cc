// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/test/run_until.h"
#include "base/test/with_feature_override.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/policy/core/common/features.h"
#include "components/policy/core/common/policy_logger.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"

class PolicyUILogsPageTest : public PlatformBrowserTest,
                             public base::test::WithFeatureOverride {
 public:
  PolicyUILogsPageTest()
      : base::test::WithFeatureOverride(
            policy::features::kPolicyPageMojoMigration) {}

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void ClickRefreshLogsButton() {
    ASSERT_TRUE(content::ExecJs(
        web_contents(), "document.querySelector('#logs-refresh').click();"));
  }

  void ClickDownloadJsonButton() {
    ASSERT_TRUE(content::ExecJs(
        web_contents(), "document.querySelector('#logs-dump').click();"));
  }

  const std::string GetPageText() {
    return content::EvalJs(web_contents(), "document.body.textContent")
        .ExtractString();
  }
};

IN_PROC_BROWSER_TEST_P(PolicyUILogsPageTest, LogsVisibleOnPage) {
#if BUILDFLAG(IS_CHROMEOS)
  GTEST_SKIP() << "Policy logging is disabled on ChromeOS.";
#else
  static constexpr char test_message[] = "This is a test!";
  LOG_POLICY(ERROR, PLATFORM_POLICY) << test_message;
  ASSERT_TRUE(content::NavigateToURL(web_contents(),
                                     GURL(chrome::kChromeUIPolicyLogsURL)));

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return GetPageText().find(test_message) != std::string::npos; }));
#endif
}

IN_PROC_BROWSER_TEST_P(PolicyUILogsPageTest, LogsRefresh) {
#if BUILDFLAG(IS_CHROMEOS)
  GTEST_SKIP() << "Policy logging is disabled on ChromeOS.";
#else
  static constexpr char test_message[] = "This is a test!";
  ASSERT_TRUE(content::NavigateToURL(web_contents(),
                                     GURL(chrome::kChromeUIPolicyLogsURL)));
  ASSERT_EQ(GetPageText().find(test_message), std::string::npos);

  LOG_POLICY(ERROR, PLATFORM_POLICY) << test_message;
  ClickRefreshLogsButton();

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return GetPageText().find(test_message) != std::string::npos; }));
#endif
}

IN_PROC_BROWSER_TEST_P(PolicyUILogsPageTest, DownloadLogs) {
#if BUILDFLAG(IS_CHROMEOS)
  GTEST_SKIP() << "Policy logging is disabled on ChromeOS.";
#else
  static constexpr char test_message[] = "This is a test!";
  LOG_POLICY(ERROR, PLATFORM_POLICY) << test_message;
  ASSERT_TRUE(content::NavigateToURL(web_contents(),
                                     GURL(chrome::kChromeUIPolicyLogsURL)));

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return GetPageText().find(test_message) != std::string::npos; }));

  content::DownloadManager* download_manager =
      web_contents()->GetBrowserContext()->GetDownloadManager();
  content::DownloadTestObserverTerminal observer(
      download_manager, 1,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);

  ClickDownloadJsonButton();

  observer.WaitForFinished();

  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  download_manager->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  ASSERT_EQ(download::DownloadItem::COMPLETE, downloads[0]->GetState());
  base::FilePath downloaded_file = downloads[0]->GetTargetFilePath();

  std::string file_contents;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::ReadFileToString(downloaded_file, &file_contents));
  }
  EXPECT_NE(file_contents.find(test_message), std::string::npos);
#endif
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PolicyUILogsPageTest);
