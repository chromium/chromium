// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_IMPL_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/values.h"
#include "chrome/test/chromedriver/chrome/chrome.h"
#include "chrome/test/chromedriver/chrome/mobile_device.h"
#include "chrome/test/chromedriver/net/sync_websocket_factory.h"

class DevToolsClient;
class DevToolsClientImpl;
class DevToolsEventListener;
class DevToolsHttpClient;
class PageTracker;
class Status;
class WebView;
class WebViewImpl;
class WebViewsInfo;
struct BrowserInfo;

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
                       const base::Value::Dict& params) override;
  Status MaximizeWindow(const std::string& target_id) override;
  Status MinimizeWindow(const std::string& target_id) override;
  Status FullScreenWindow(const std::string& target_id) override;
  Status CloseWebView(const std::string& id) override;
  Status ActivateWebView(const std::string& id) override;
  Status SetAcceptInsecureCerts() override;
  Status SetPermission(std::unique_ptr<base::Value::Dict> permission_descriptor,
                       PermissionState desired_state,
                       WebView* current_view) override;
  bool IsMobileEmulationEnabled() const override;
  bool HasTouchScreen() const override;
  std::string page_load_strategy() const override;
  Status Quit() override;
  DevToolsClient* Client() const;

 protected:
  ChromeImpl(std::unique_ptr<DevToolsHttpClient> http_client,
             std::unique_ptr<DevToolsClient> websocket_client,
             std::vector<std::unique_ptr<DevToolsEventListener>>
                 devtools_event_listeners,
             absl::optional<MobileDevice> mobile_device,
             SyncWebSocketFactory socket_factory,
             std::string page_load_strategy);

  virtual Status QuitImpl() = 0;

  Status CreateClient(const std::string& id,
                      std::unique_ptr<DevToolsClientImpl>* client);
  Status CloseTarget(const std::string& id);

  struct Window {
    int id;
    std::string state;
    int left;
    int top;
    int width;
    int height;
  };
  virtual Status GetWindow(const std::string& target_id, Window* window);
  Status ParseWindow(const base::Value::Dict& params, Window* window);
  Status ParseWindowBounds(const base::Value::Dict& params, Window* window);
  Status GetWindowBounds(int window_id, Window* window);
  Status SetWindowBounds(Window* window,
                         const std::string& target_id,
                         std::unique_ptr<base::Value::Dict> bounds);
  Status GetWebViewsInfo(WebViewsInfo* views_info);

  bool quit_ = false;
  absl::optional<MobileDevice> mobile_device_;
  SyncWebSocketFactory socket_factory_;
  std::unique_ptr<DevToolsHttpClient> devtools_http_client_;
  std::unique_ptr<DevToolsClient> devtools_websocket_client_;

 private:
  static Status PermissionNameToChromePermissions(
      const base::Value::Dict& permission_descriptor,
      Chrome::PermissionState setting,
      std::vector<std::string>* chrome_permissions);

  Status UpdateWebViews(const WebViewsInfo& views_info, bool w3c_compliant);

  // Web views in this list are in the same order as they are opened.
  std::list<std::unique_ptr<WebViewImpl>> web_views_;
  std::unique_ptr<PageTracker> page_tracker_;
  std::vector<std::unique_ptr<DevToolsEventListener>> devtools_event_listeners_;
  std::string page_load_strategy_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_IMPL_H_
