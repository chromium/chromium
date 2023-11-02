// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/chromeos/sharesheet_client.h"

#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/filename_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"
#include "url/gurl.h"

namespace webshare {

class SharesheetClientBrowserTest : public InProcessBrowserTest {
 public:
  SharesheetClientBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kWebShare);
  }

  static void CheckSize(const base::FilePath& file_path,
                        int64_t expected_size) {
    base::RunLoop run_loop;
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
        base::BindLambdaForTesting([&file_path]() {
          base::ScopedBlockingCall scoped_blocking_call(
              FROM_HERE, base::BlockingType::WILL_BLOCK);
          int64_t file_size;
          EXPECT_TRUE(base::GetFileSize(file_path, &file_size));
          return file_size;
        }),
        base::BindLambdaForTesting(
            [&run_loop, &expected_size](int64_t file_size) {
              EXPECT_EQ(expected_size, file_size);
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  GURL GetAppUrl() const {
    return embedded_test_server()->GetURL("/webshare/index.html");
  }

  void ConfirmShareText(
      const std::string& script,
      const char* expected_text,
      const char* expected_title,
      const std::vector<std::string>& expected_content_types) {
    SharesheetClient::SetSharesheetCallbackForTesting(
        base::BindLambdaForTesting(
            [&expected_text, &expected_title, &expected_content_types](
                content::WebContents* in_contents,
                const std::vector<base::FilePath>& file_paths,
                const std::vector<std::string>& content_types,
                const std::vector<uint64_t>& file_sizes,
                const std::string& text, const std::string& title,
                SharesheetClient::DeliveredCallback delivered_callback) {
              EXPECT_EQ(text, expected_text);
              EXPECT_EQ(title, expected_title);
              EXPECT_EQ(file_paths.size(), content_types.size());
              EXPECT_EQ(content_types, expected_content_types);
              std::move(delivered_callback)
                  .Run(sharesheet::SharesheetResult::kSuccess);
            }));

    content::WebContents* const contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ("share succeeded", content::EvalJs(contents, script));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SharesheetClientBrowserTest, ShareMultipleFiles) {
  const std::string script = "share_multiple_files()";
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetAppUrl()));
  content::WebContents* const contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::vector<base::FilePath> file_paths;

  SharesheetClient::SetSharesheetCallbackForTesting(base::BindLambdaForTesting(
      [contents, &file_paths](
          content::WebContents* in_contents,
          const std::vector<base::FilePath>& in_file_paths,
          const std::vector<std::string>& content_types,
          const std::vector<uint64_t>& file_sizes, const std::string& text,
          const std::string& title,
          SharesheetClient::DeliveredCallback delivered_callback) {
        EXPECT_EQ(contents, in_contents);

        file_paths = std::move(in_file_paths);

        EXPECT_EQ(content_types.size(), 3U);
        EXPECT_EQ(content_types[0], "audio/mpeg");
        EXPECT_EQ(content_types[1], "video/mp4");
        EXPECT_EQ(content_types[2], "image/gif");

        std::move(delivered_callback)
            .Run(sharesheet::SharesheetResult::kSuccess);
      }));

  EXPECT_EQ("share succeeded", content::EvalJs(contents, script));
  EXPECT_EQ(file_paths.size(), 3U);

  const base::FilePath share_cache =
      file_manager::util::GetShareCacheFilePath(browser()->profile());
  EXPECT_EQ(file_paths[0],
            share_cache.AppendASCII(".WebShare/share1/sam.ple.mp3"));
  EXPECT_EQ(file_paths[1],
            share_cache.AppendASCII(".WebShare/share2/sample.mp4"));
  EXPECT_EQ(file_paths[2],
            share_cache.AppendASCII(".WebShare/share3/sam_ple.gif"));

  CheckSize(file_paths[0], /*expected_size=*/345);
  CheckSize(file_paths[1], /*expected_size=*/67890);
  CheckSize(file_paths[2], /*expected_size=*/1);
}

IN_PROC_BROWSER_TEST_F(SharesheetClientBrowserTest, RepeatedShare) {
  const int kRepeats = 3;
  const std::string script = "share_single_file()";
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetAppUrl()));
  content::WebContents* const contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  for (int count = 0; count < kRepeats; ++count) {
    std::vector<base::FilePath> file_paths;

    SharesheetClient::SetSharesheetCallbackForTesting(
        base::BindLambdaForTesting(
            [contents, &file_paths](
                content::WebContents* in_contents,
                const std::vector<base::FilePath>& in_file_paths,
                const std::vector<std::string>& content_types,
                const std::vector<uint64_t>& file_sizes,
                const std::string& text, const std::string& title,
                SharesheetClient::DeliveredCallback delivered_callback) {
              EXPECT_EQ(contents, in_contents);

              file_paths = std::move(in_file_paths);

              EXPECT_EQ(content_types.size(), 1U);
              EXPECT_EQ(content_types[0], "image/webp");

              std::move(delivered_callback)
                  .Run(sharesheet::SharesheetResult::kSuccess);
            }));

    EXPECT_EQ("share succeeded", content::EvalJs(contents, script));
    EXPECT_EQ(file_paths.size(), 1U);
    CheckSize(file_paths[0], /*expected_size=*/12);
  }
}

IN_PROC_BROWSER_TEST_F(SharesheetClientBrowserTest, CancelledShare) {
  const std::string script = "share_single_file()";
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* const contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetAppUrl()));
  SharesheetClient::SetSharesheetCallbackForTesting(base::BindLambdaForTesting(
      [](content::WebContents* in_contents,
         const std::vector<base::FilePath>& file_paths,
         const std::vector<std::string>& content_types,
         const std::vector<uint64_t>& file_sizes, const std::string& text,
         const std::string& title,
         SharesheetClient::DeliveredCallback delivered_callback) {
        std::move(delivered_callback)
            .Run(sharesheet::SharesheetResult::kCancel);
      }));

  EXPECT_EQ("share failed: AbortError: Share canceled",
            content::EvalJs(contents, script));
}

IN_PROC_BROWSER_TEST_F(SharesheetClientBrowserTest, Text) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetAppUrl()));
  ConfirmShareText("share_title()",
                   /*expected_text=*/"",
                   /*expected_title=*/"Subject", /*expected_content_types=*/{});
  ConfirmShareText("share_title_url()",
                   /*expected_text=*/"https://example.com/",
                   /*expected_title=*/"Subject", /*expected_content_types=*/{});
  ConfirmShareText("share_text()",
                   /*expected_text=*/"Message",
                   /*expected_title=*/"", /*expected_content_types=*/{});
  ConfirmShareText("share_text_url()",
                   /*expected_text=*/"Message https://example.com/",
                   /*expected_title=*/"", /*expected_content_types=*/{});
  ConfirmShareText("share_url()",
                   /*expected_text=*/"https://example.com/",
                   /*expected_title=*/"", /*expected_content_types=*/{});
}

IN_PROC_BROWSER_TEST_F(SharesheetClientBrowserTest, TextWithFile) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetAppUrl()));
  const std::vector<std::string> expected_content_types{"image/webp"};
  ConfirmShareText("share_file_title()",
                   /*expected_text=*/"",
                   /*expected_title=*/"Subject", expected_content_types);
  ConfirmShareText("share_file_title_url()",
                   /*expected_text=*/"https://example.com/",
                   /*expected_title=*/"Subject", expected_content_types);
  ConfirmShareText("share_file_text()",
                   /*expected_text=*/"Message",
                   /*expected_title=*/"", expected_content_types);
  ConfirmShareText("share_file_text_url()",
                   /*expected_text=*/"Message https://example.com/",
                   /*expected_title=*/"", expected_content_types);
  ConfirmShareText("share_file_url()",
                   /*expected_text=*/"https://example.com/",
                   /*expected_title=*/"", expected_content_types);
}

}  // namespace webshare
