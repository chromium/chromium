// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_IMPL_H_

#include <list>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/values.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/chrome.h"
#include "chrome/test/chromedriver/chrome/devtools_http_client.h"
#include "chrome/test/chromedriver/chrome/mobile_device.h"

class DevToolsClient;
class DevToolsEventListener;
class PageTracker;
class Status;
class WebView;
class WebViewImpl;

namespace internal {
struct Position {
  int left = 0;
  int top = 0;
};

struct Size {
  int width = 0;
  int height = 0;
};

struct Window {
  int id;
  std::string state;
  int left;
  int top;
  int width;
  int height;
};

struct WindowBounds {
  WindowBounds();
  ~WindowBounds();
  std::optional<Position> position;
  std::optional<Size> size;
  std::optional<std::string> state;
  base::Value::Dict ToDict() const;
  bool Matches(const Window& window) const;
};
}  // namespace internal

class ChromeImpl : public Chrome {
 public:
  ~ChromeImpl() override;
  // Overridden from Chrome:
  Status GetAsDesktop(ChromeDesktopImpl** desktop) override;
  const BrowserInfo* GetBrowserInfo() const override;
  bool HasCrashedWebView() override;
  Status GetWebViewCount(size_t* web_view_count, bool w3c_compliant) override;
  Status GetWebViewIdForFirstTab(std::string* web_view_id,
                                 bool w3c_complaint) override;
  Status GetWebViewIds(std::list<std::string>* web_view_ids,
                       bool w3c_compliant) override;
  Status GetWebViewById(const std::string& id, WebView** web_view) override;
  Status NewWindow(const std::string& target_id,
                   WindowType type,
                   bool is_background,
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
  ChromeImpl(BrowserInfo browser_info,
             std::set<WebViewInfo::Type> window_types,
             std::unique_ptr<DevToolsClient> websocket_client,
             std::vector<std::unique_ptr<DevToolsEventListener>>
                 devtools_event_listeners,
             std::optional<MobileDevice> mobile_device,
             std::string page_load_strategy,
             bool autoaccept_beforeunload);

  virtual Status QuitImpl() = 0;
  Status CloseTarget(const std::string& id);

  bool IsBrowserWindow(const WebViewInfo& view) const;

  virtual Status GetWindow(const std::string& target_id,
                           internal::Window& window);
  Status ParseWindow(const base::Value::Dict& params, internal::Window& window);
  Status ParseWindowBounds(const base::Value::Dict& params,
                           internal::Window& window);
  Status GetWindowBounds(int window_id, internal::Window& window);
  Status SetWindowBounds(internal::Window window,
                         const std::string& target_id,
                         const internal::WindowBounds& bounds);

  bool quit_ = false;
  std::optional<MobileDevice> mobile_device_;
  BrowserInfo browser_info_;
  std::set<WebViewInfo::Type> window_types_;
  std::unique_ptr<DevToolsClient> devtools_websocket_client_;
  bool autoaccept_beforeunload_ = false;

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
