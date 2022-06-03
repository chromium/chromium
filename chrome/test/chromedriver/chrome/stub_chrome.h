// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_STUB_CHROME_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_STUB_CHROME_H_

#include <list>
#include <memory>

#include "base/compiler_specific.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/chrome.h"

class Status;
class WebView;

class StubChrome : public Chrome {
 public:
  StubChrome();
  ~StubChrome() override;

  // Overridden from Chrome:
  Status GetAsDesktop(ChromeDesktopImpl** desktop) override;
  const BrowserInfo* GetBrowserInfo() const override;
  bool HasCrashedWebView() override;
  Status GetWebViewIdForFirstTab(std::string* web_view_id,
                                 bool w3c_compliant) override;
  Status GetWebViewIds(std::list<std::string>* web_view_ids,
                       bool w3c_compliant) override;
  Status GetWebViewById(const std::string& id, WebView** web_view) override;
  Status NewWindow(const std::string& target_id,
                   WindowType type,
                   std::string* window_handle) override;
  Status GetWindowRect(const std::string& id, WindowRect* rect) override;
  Status SetWindowRect(const std::string& target_id,
                       const base::DictionaryValue& params) override;
  Status MaximizeWindow(const std::string& target_id) override;
  Status MinimizeWindow(const std::string& target_id) override;
  Status FullScreenWindow(const std::string& target_id) override;
  Status CloseWebView(const std::string& id) override;
  Status ActivateWebView(const std::string& id) override;
  Status SetAcceptInsecureCerts() override;
  Status SetPermission(
      std::unique_ptr<base::DictionaryValue> permission_descriptor,
      Chrome::PermissionState desired_state,
      bool one_realm,
      WebView* current_view) override;
  std::string GetOperatingSystemName() override;
  bool IsMobileEmulationEnabled() const override;
  bool HasTouchScreen() const override;
  std::string page_load_strategy() const override;
  Status Quit() override;

 private:
  BrowserInfo browser_info_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_STUB_CHROME_H_
