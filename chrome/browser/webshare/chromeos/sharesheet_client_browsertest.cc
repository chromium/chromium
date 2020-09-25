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
  base::FilePath first_file_path;
  base::FilePath second_file_path;

  SharesheetClient::SetSharesheetCallbackForTesting(base::BindLambdaForTesting(
      [contents, &first_file_path, &second_file_path](
          content::WebContents* web_contents, std::vector<GURL> file_urls,
          std::vector<std::string> content_types) {
        EXPECT_EQ(contents, web_contents);

        EXPECT_EQ(file_urls.size(), 2U);
        EXPECT_TRUE(net::FileURLToFilePath(file_urls[0], &first_file_path));
        EXPECT_TRUE(net::FileURLToFilePath(file_urls[1], &second_file_path));

        EXPECT_EQ(content_types.size(), 2U);
        EXPECT_EQ(content_types[0], "audio/mp3");
        EXPECT_EQ(content_types[1], "video/mp4");

        return blink::mojom::ShareError::OK;
      }));

  EXPECT_EQ("share succeeded", content::EvalJs(contents, script));
  CheckSize(first_file_path, /*expected_size=*/345);
  CheckSize(second_file_path, /*expected_size=*/67890);
}

IN_PROC_BROWSER_TEST_F(SharesheetClientBrowserTest, RepeatedShare) {
  const int kRepeats = 3;
  const std::string script = "share_single_file()";
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(browser(), GetAppUrl());
  content::WebContents* const contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  for (int count = 0; count < kRepeats; ++count) {
    base::FilePath file_path;

    SharesheetClient::SetSharesheetCallbackForTesting(
        base::BindLambdaForTesting(
            [contents, &file_path](content::WebContents* web_contents,
                                   std::vector<GURL> file_urls,
                                   std::vector<std::string> content_types) {
              EXPECT_EQ(contents, web_contents);

              EXPECT_EQ(file_urls.size(), 1U);
              EXPECT_TRUE(net::FileURLToFilePath(file_urls[0], &file_path));

              EXPECT_EQ(content_types.size(), 1U);
              EXPECT_EQ(content_types[0], "image/webp");

              return blink::mojom::ShareError::OK;
            }));

    EXPECT_EQ("share succeeded", content::EvalJs(contents, script));
    CheckSize(file_path, /*expected_size=*/12);
  }
}

}  // namespace webshare
