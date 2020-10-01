// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/chromeos/sharesheet_client.h"

#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/scoped_blocking_call.h"
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

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SharesheetClientBrowserTest, ShareTwoFiles) {
  const std::string script = "share_multiple_files()";
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(browser(), GetAppUrl());
  content::WebContents* const contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::vector<base::FilePath> file_paths;

  SharesheetClient::SetSharesheetCallbackForTesting(base::BindLambdaForTesting(
      [contents, &file_paths](content::WebContents* in_contents,
                              std::vector<base::FilePath> in_file_paths,
                              std::vector<std::string> content_types,
                              SharesheetClient::CloseCallback close_callback) {
        EXPECT_EQ(contents, in_contents);

        file_paths = std::move(in_file_paths);

        EXPECT_EQ(content_types.size(), 2U);
        EXPECT_EQ(content_types[0], "audio/mp3");
        EXPECT_EQ(content_types[1], "video/mp4");

        std::move(close_callback).Run(sharesheet::SharesheetResult::kSuccess);
      }));

  EXPECT_EQ("share succeeded", content::EvalJs(contents, script));
  EXPECT_EQ(file_paths.size(), 2U);
  CheckSize(file_paths[0], /*expected_size=*/345);
  CheckSize(file_paths[1], /*expected_size=*/67890);
}

IN_PROC_BROWSER_TEST_F(SharesheetClientBrowserTest, RepeatedShare) {
  const int kRepeats = 3;
  const std::string script = "share_single_file()";
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(browser(), GetAppUrl());
  content::WebContents* const contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  for (int count = 0; count < kRepeats; ++count) {
    std::vector<base::FilePath> file_paths;

    SharesheetClient::SetSharesheetCallbackForTesting(
        base::BindLambdaForTesting(
            [contents, &file_paths](
                content::WebContents* in_contents,
                std::vector<base::FilePath> in_file_paths,
                std::vector<std::string> content_types,
                SharesheetClient::CloseCallback close_callback) {
              EXPECT_EQ(contents, in_contents);

              file_paths = std::move(in_file_paths);

              EXPECT_EQ(content_types.size(), 1U);
              EXPECT_EQ(content_types[0], "image/webp");

              std::move(close_callback)
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

  ui_test_utils::NavigateToURL(browser(), GetAppUrl());
  SharesheetClient::SetSharesheetCallbackForTesting(base::BindLambdaForTesting(
      [](content::WebContents* in_contents,
         std::vector<base::FilePath> file_paths,
         std::vector<std::string> content_types,
         SharesheetClient::CloseCallback close_callback) {
        std::move(close_callback).Run(sharesheet::SharesheetResult::kCancel);
      }));

  EXPECT_EQ("share failed: AbortError: Share canceled",
            content::EvalJs(contents, script));
}

}  // namespace webshare
