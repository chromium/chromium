// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_BROWSER_TEST_UTILS_H_
#define CHROME_BROWSER_USB_USB_BROWSER_TEST_UTILS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/usb/chrome_usb_delegate.h"

class TestUsbDelegate : public ChromeUsbDelegate {
 public:
  TestUsbDelegate();
  TestUsbDelegate(const TestUsbDelegate&) = delete;
  TestUsbDelegate& operator=(const TestUsbDelegate&) = delete;
  ~TestUsbDelegate() override;

  std::unique_ptr<content::UsbChooser> RunChooser(
      content::RenderFrameHost& frame,
      blink::mojom::WebUsbRequestDeviceOptionsPtr options,
      blink::mojom::WebUsbService::GetPermissionCallback callback) override;

  void UseFakeChooser() { use_fake_chooser_ = true; }

 private:
  bool use_fake_chooser_ = false;
};

class TestUsbContentBrowserClient : public ChromeContentBrowserClient {
 public:
  TestUsbContentBrowserClient();
  TestUsbContentBrowserClient(const TestUsbContentBrowserClient&) = delete;
  TestUsbContentBrowserClient& operator=(const TestUsbContentBrowserClient&) =
      delete;
  ~TestUsbContentBrowserClient() override;

  // ChromeContentBrowserClient:
  content::UsbDelegate* GetUsbDelegate() override;

  TestUsbDelegate& delegate() { return *usb_delegate_; }

  void SetAsBrowserClient();
  void UnsetAsBrowserClient();

 private:
  std::unique_ptr<TestUsbDelegate> usb_delegate_;
  raw_ptr<content::ContentBrowserClient> original_content_browser_client_;
};

#endif  // CHROME_BROWSER_USB_USB_BROWSER_TEST_UTILS_H_
