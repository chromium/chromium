// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_IMPL_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/chrome.h"

struct BrowserInfo;
class DevToolsClient;
class DevToolsEventListener;
class DevToolsHttpClient;
class Status;
class WebView;
class WebViewImpl;
class WebViewsInfo;

class ChromeImpl : public Chrome {
 public:
  ~ChromeImpl() override;
  // Overridden from Chrome:
  Status GetAsDesktop(ChromeDesktopImpl** desktop) override;
  const BrowserInfo* GetBrowserInfo() const override;
  bool HasCrashedWebView() override;
  Status GetWebViewIdForFirstTab(std::string* web_view_id,
                                 bool w3c_complaint) override;
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
      PermissionState desired_state,
      bool unused_one_realm,
      WebView* current_view) override;
  bool IsMobileEmulationEnabled() const override;
  bool HasTouchScreen() const override;
  std::string page_load_strategy() const override;
  Status Quit() override;

 protected:
  ChromeImpl(std::unique_ptr<DevToolsHttpClient> http_client,
             std::unique_ptr<DevToolsClient> websocket_client,
             std::vector<std::unique_ptr<DevToolsEventListener>>
                 devtools_event_listeners,
             std::string page_load_strategy);

  virtual Status QuitImpl() = 0;

  struct Window {
    int id;
    std::string state;
    int left;
    int top;
    int width;
    int height;
  };
  virtual Status GetWindow(const std::string& target_id, Window* window);
  Status ParseWindow(std::unique_ptr<base::DictionaryValue> params,
                     Window* window);
  Status ParseWindowBounds(std::unique_ptr<base::DictionaryValue> params,
                           Window* window);
  Status GetWindowBounds(int window_id, Window* window);
  Status SetWindowBounds(Window* window,
                         const std::string& target_id,
                         std::unique_ptr<base::DictionaryValue> bounds);

  bool quit_;
  std::unique_ptr<DevToolsHttpClient> devtools_http_client_;
  std::unique_ptr<DevToolsClient> devtools_websocket_client_;

 private:
  static Status PermissionNameToChromePermissions(
      const base::DictionaryValue& permission_descriptor,
      Chrome::PermissionState setting,
      std::vector<std::string>* chrome_permissions);

  void UpdateWebViews(const WebViewsInfo& views_info, bool w3c_compliant);

  // Web views in this list are in the same order as they are opened.
  std::list<std::unique_ptr<WebViewImpl>> web_views_;
  std::vector<std::unique_ptr<DevToolsEventListener>> devtools_event_listeners_;
  std::string page_load_strategy_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_IMPL_H_
