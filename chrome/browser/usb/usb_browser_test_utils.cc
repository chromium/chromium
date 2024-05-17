// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_browser_test_utils.h"

#include "chrome/browser/usb/usb_chooser_controller.h"
#include "chrome/browser/usb/web_usb_chooser.h"
#include "components/permissions/chooser_controller.h"
#include "content/public/common/content_client.h"

namespace {

class FakeChooserView : public permissions::ChooserController::View {
 public:
  explicit FakeChooserView(
      std::unique_ptr<permissions::ChooserController> controller)
      : controller_(std::move(controller)) {
    controller_->set_view(this);
  }

  FakeChooserView(const FakeChooserView&) = delete;
  FakeChooserView& operator=(const FakeChooserView&) = delete;

  ~FakeChooserView() override { controller_->set_view(nullptr); }

  void OnOptionsInitialized() override {
    if (controller_->NumOptions()) {
      controller_->Select({0});
    } else {
      controller_->Cancel();
    }
    delete this;
  }

  void OnOptionAdded(size_t index) override { NOTREACHED_IN_MIGRATION(); }
  void OnOptionRemoved(size_t index) override { NOTREACHED_IN_MIGRATION(); }
  void OnOptionUpdated(size_t index) override { NOTREACHED_IN_MIGRATION(); }
  void OnAdapterEnabledChanged(bool enabled) override {
    NOTREACHED_IN_MIGRATION();
  }
  void OnRefreshStateChanged(bool refreshing) override {
    NOTREACHED_IN_MIGRATION();
  }

 private:
  std::unique_ptr<permissions::ChooserController> controller_;
};

class FakeUsbChooser : public WebUsbChooser {
 public:
  FakeUsbChooser() = default;
  FakeUsbChooser(const FakeUsbChooser&) = delete;
  FakeUsbChooser& operator=(const FakeUsbChooser&) = delete;
  ~FakeUsbChooser() override = default;

  void ShowChooser(content::RenderFrameHost* frame,
                   std::unique_ptr<UsbChooserController> controller) override {
    // Device list initialization in UsbChooserController may complete before
    // having a valid view in which case OnOptionsInitialized() has no chance to
    // be triggered, so select the first option directly if options are ready.
    if (controller->NumOptions()) {
      controller->Select({0});
    } else {
      new FakeChooserView(std::move(controller));
    }
  }
};

}  // namespace

TestUsbDelegate::TestUsbDelegate() = default;
TestUsbDelegate::~TestUsbDelegate() = default;

std::unique_ptr<content::UsbChooser> TestUsbDelegate::RunChooser(
    content::RenderFrameHost& frame,
    blink::mojom::WebUsbRequestDeviceOptionsPtr options,
    blink::mojom::WebUsbService::GetPermissionCallback callback) {
  if (use_fake_chooser_) {
    auto chooser = std::make_unique<FakeUsbChooser>();
    chooser->ShowChooser(&frame,
                         std::make_unique<UsbChooserController>(
                             &frame, std::move(options), std::move(callback)));
    return chooser;
  } else {
    return ChromeUsbDelegate::RunChooser(frame, std::move(options),
                                         std::move(callback));
  }
}

TestUsbContentBrowserClient::TestUsbContentBrowserClient()
    : usb_delegate_(std::make_unique<TestUsbDelegate>()) {}
TestUsbContentBrowserClient::~TestUsbContentBrowserClient() = default;

content::UsbDelegate* TestUsbContentBrowserClient::GetUsbDelegate() {
  return usb_delegate_.get();
}

void TestUsbContentBrowserClient::SetAsBrowserClient() {
  original_content_browser_client_ = content::SetBrowserClientForTesting(this);
}

void TestUsbContentBrowserClient::UnsetAsBrowserClient() {
  content::SetBrowserClientForTesting(original_content_browser_client_);
  usb_delegate_.reset();
}
