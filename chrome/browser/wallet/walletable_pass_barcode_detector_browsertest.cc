// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/content/browser/walletable_pass_barcode_detector.h"

#include <memory>
#include <optional>

#include "base/base64.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/wallet/content/browser/walletable_pass_barcode_detector_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace wallet {

namespace {

// A base64 encoded 20x20 black PNG image.
const char kBlackImage[] =
    "iVBORw0KGgoAAAANSUhEUgAAABQAAAAUCAIAAAAC64paAAAAEklEQVR4nGNgGAWjYBSMgqELAA"
    "TEAAE0eCSYAAAAAElFTkSuQmCC";
}  // namespace

class WalletablePassBarcodeDetectorBrowserTest : public InProcessBrowserTest {
 public:
  WalletablePassBarcodeDetectorBrowserTest() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "content/test/data/media");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

// Tests that no barcodes are detected in a simple black image.
IN_PROC_BROWSER_TEST_F(WalletablePassBarcodeDetectorBrowserTest,
                       NoBarcodesInImages) {
  const GURL url(
      "data:text/html;charset=utf-8,"
      "<body><img src='data:image/png;base64," +
      std::string(kBlackImage) + "'></body>");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  base::test::TestFuture<const std::vector<wallet::WalletBarcode>&> future;
  auto detector = std::make_unique<WalletablePassBarcodeDetectorImpl>();
  detector->Detect(GetWebContents(), future.GetCallback());

  // TODO(crbug.com/438364540): Current implementation always return empty list.
  // Need to add another real test for barcode detection.
  EXPECT_THAT(future.Get(), testing::IsEmpty());
}

// Tests that the barcode detection gracefully handles the case where the image
// extractor disconnects, for example, due to a tab closure.
IN_PROC_BROWSER_TEST_F(WalletablePassBarcodeDetectorBrowserTest,
                       ImageExtractorDisconnects) {
  const GURL url("data:text/html,<body><p>hello</p></body>");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  base::test::TestFuture<const std::vector<WalletBarcode>&> future;
  auto detector = std::make_unique<WalletablePassBarcodeDetectorImpl>();
  detector->Detect(GetWebContents(), future.GetCallback());

  // Close the tab, which should disconnect the ImageLoader pipe.
  GetWebContents()->Close();

  // The disconnect handler should run the callback and delete the `detector`.
  EXPECT_THAT(future.Get(), testing::IsEmpty());
}

}  // namespace wallet
