// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/usb/chrome_usb_delegate.h"
#include "chrome/browser/usb/usb_chooser_controller.h"
#include "chrome/browser/usb/web_usb_chooser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace {

constexpr int kMicroBitVendorId = 0x0d28;
constexpr int kMicroBitProductId = 0x0204;

// The chooser view that will select the first device in the list upon the list
// initialization.
class UsbRealTargetTestChooserView
    : public permissions::ChooserController::View {
 public:
  explicit UsbRealTargetTestChooserView(
      std::unique_ptr<permissions::ChooserController> controller)
      : controller_(std::move(controller)) {
    controller_->set_view(this);
  }

  UsbRealTargetTestChooserView(const UsbRealTargetTestChooserView&) = delete;
  UsbRealTargetTestChooserView& operator=(const UsbRealTargetTestChooserView&) =
      delete;

  ~UsbRealTargetTestChooserView() override { controller_->set_view(nullptr); }

  void OnOptionsInitialized() override { controller_->Select({0}); }

  void OnOptionAdded(size_t index) override {}
  void OnOptionRemoved(size_t index) override {}
  void OnOptionUpdated(size_t index) override {}
  void OnAdapterEnabledChanged(bool enabled) override {}
  void OnRefreshStateChanged(bool refreshing) override {}

 private:
  std::unique_ptr<permissions::ChooserController> controller_;
};

class UsbRealTargetTestChooser : public WebUsbChooser {
 public:
  UsbRealTargetTestChooser() = default;
  UsbRealTargetTestChooser(const UsbRealTargetTestChooser&) = delete;
  UsbRealTargetTestChooser& operator=(const UsbRealTargetTestChooser&) = delete;
  ~UsbRealTargetTestChooser() override = default;

  void ShowChooser(content::RenderFrameHost* frame,
                   std::unique_ptr<UsbChooserController> controller) override {
    view_ =
        std::make_unique<UsbRealTargetTestChooserView>(std::move(controller));
  }

 private:
  std::unique_ptr<UsbRealTargetTestChooserView> view_;
};

class UsbRealTargetTestDelegate : public ChromeUsbDelegate {
 public:
  UsbRealTargetTestDelegate() = default;
  ~UsbRealTargetTestDelegate() override = default;

  std::unique_ptr<content::UsbChooser> RunChooser(
      content::RenderFrameHost& frame,
      blink::mojom::WebUsbRequestDeviceOptionsPtr options,
      blink::mojom::WebUsbService::GetPermissionCallback callback) override {
    auto chooser = std::make_unique<UsbRealTargetTestChooser>();
    chooser->ShowChooser(&frame,
                         std::make_unique<UsbChooserController>(
                             &frame, std::move(options), std::move(callback)));
    return chooser;
  }
};

class TestUsbContentBrowserClient : public content::ContentBrowserClient {
 public:
  TestUsbContentBrowserClient()
      : usb_delegate_(std::make_unique<UsbRealTargetTestDelegate>()) {}
  ~TestUsbContentBrowserClient() override = default;

  content::UsbDelegate* GetUsbDelegate() override {
    return usb_delegate_.get();
  }

  void SetAsBrowserClient() {
    original_content_browser_client_ =
        content::SetBrowserClientForTesting(this);
  }

  void UnsetAsBrowserClient() {
    content::SetBrowserClientForTesting(original_content_browser_client_);
    usb_delegate_.reset();
  }

 private:
  std::unique_ptr<UsbRealTargetTestDelegate> usb_delegate_;
  raw_ptr<content::ContentBrowserClient> original_content_browser_client_ =
      nullptr;
};

class UsbRealTargetTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());

    test_content_browser_client_.SetAsBrowserClient();
    GURL url = embedded_test_server()->GetURL("localhost", "/simple_page.html");
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    content::RenderFrameHost* render_frame_host = browser()
                                                      ->tab_strip_model()
                                                      ->GetActiveWebContents()
                                                      ->GetPrimaryMainFrame();
    EXPECT_EQ(url.DeprecatedGetOriginAsURL(),
              render_frame_host->GetLastCommittedOrigin().GetURL());
  }

  void TearDownOnMainThread() override {
    test_content_browser_client_.UnsetAsBrowserClient();
  }

 private:
  TestUsbContentBrowserClient test_content_browser_client_;
};

IN_PROC_BROWSER_TEST_F(UsbRealTargetTest, OpenAndCloseDevice) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto test_script = base::StringPrintf(
      R"((async () => {
        const device = await navigator.usb.requestDevice({
          filters: [{ vendorId: %d, productId: %d }]
        });
        await device.open();
        const result = device.opened;
        await device.close();
        return result;
      })())",
      kMicroBitVendorId, kMicroBitProductId);
  EXPECT_EQ(true, EvalJs(web_contents, test_script));
}

}  // namespace
