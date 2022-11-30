// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/mac/sharing_service_operation.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"
#include "url/gurl.h"

namespace webshare {

class SharingServiceOperationBrowserTest : public InProcessBrowserTest {
 public:
  SharingServiceOperationBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kWebShare);
  }

  GURL GetAppUrl() const {
    return embedded_test_server()->GetURL("/webshare/index.html");
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SharingServiceOperationBrowserTest,
                       ShareIllegalFilename) {
  const std::string script =
      "share_multiple_custom_files_url('sample.csv', '..sample.csv')";
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetAppUrl()));
  content::WebContents* const contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::vector<base::FilePath> file_paths;

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetAppUrl()));
  SharingServiceOperation::SetSharePickerCallbackForTesting(
      base::BindLambdaForTesting(
          [](content::WebContents* in_contents,
             const std::vector<base::FilePath>& file_paths,
             const std::string& text, const std::string& title, const GURL& url,
             blink::mojom::ShareService::ShareCallback close_callback) {
            EXPECT_EQ(file_paths[0].BaseName().value(), "sample.csv");
            EXPECT_EQ(file_paths[1].BaseName().value(), "_.sample.csv");
            std::move(close_callback).Run(blink::mojom::ShareError::OK);
          }));

  EXPECT_EQ("share succeeded", content::EvalJs(contents, script));
}

}  // namespace webshare
