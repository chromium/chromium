// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_H_

#include <list>
#include <string>

#include "base/values.h"

struct BrowserInfo;
class ChromeDesktopImpl;
class Status;
class WebView;

class Chrome {
 public:
  enum class WindowType {
    kWindow,
    kTab,
  };

  enum class PermissionState {
    kGranted,
    kDenied,
    kPrompt,
  };

  struct WindowRect {
    int x;
    int y;
    int width;
    int height;
  };

  virtual ~Chrome() = default;

  virtual Status GetAsDesktop(ChromeDesktopImpl** desktop) = 0;

  virtual const BrowserInfo* GetBrowserInfo() const = 0;

  virtual bool HasCrashedWebView() = 0;

  // Return number of opened WebViews without updating the internal maps
  // it's needed for BiDi to prevent trying to attach to sessions when
  // classic commands are not used.
  virtual Status GetWebViewCount(size_t* web_view_count,
                                 bool w3c_compliant) = 0;

  // Return the id of the first WebView that is a page.
  virtual Status GetWebViewIdForFirstTab(std::string* web_view_id,
                                         bool w3c_compliant) = 0;

  // Return ids of opened WebViews. The list is not guaranteed to be in the same
  // order as those WebViews are opened, if two or more new windows are opened
  // between two calls of this method.
  virtual Status GetWebViewIds(std::list<std::string>* web_view_ids,
                               bool w3c_compliant) = 0;

  // Return the WebView for the given id.
  virtual Status GetWebViewById(const std::string& id, WebView** web_view) = 0;

  // Makes new window or tab.
  virtual Status NewWindow(const std::string& target_id,
                           WindowType type,
                           bool is_background,
                           std::string* window_handle) = 0;

  // Gets the rect of the specified WebView
  virtual Status GetWindowRect(const std::string& id, WindowRect* rect) = 0;

  // Sets the rect of the specified WebView
  virtual Status SetWindowRect(const std::string& target_id,
                               const base::Value::Dict& params) = 0;

  // Maximizes specified WebView.
  virtual Status MaximizeWindow(const std::string& target_id) = 0;

  // Minimizes specified WebView.
  virtual Status MinimizeWindow(const std::string& target_id) = 0;

  // Opens specified WebView in full screen mode.
  virtual Status FullScreenWindow(const std::string& target_id) = 0;

  // Closes the specified WebView.
  virtual Status CloseWebView(const std::string& id) = 0;

  // Activates the specified WebView.
  virtual Status ActivateWebView(const std::string& id) = 0;

  // Enables acceptInsecureCerts mode for the browser.
  virtual Status SetAcceptInsecureCerts() = 0;

  // Requests altering permission setting for given permission.
  virtual Status SetPermission(
      std::unique_ptr<base::Value::Dict> permission_descriptor,
      PermissionState desired_state,
      WebView* current_view) = 0;

  // Get the operation system where Chrome is running.
  virtual std::string GetOperatingSystemName() = 0;

  // Return whether the mobileEmulation capability has been enabled.
  virtual bool IsMobileEmulationEnabled() const = 0;

  // Return whether the target device has a touchscreen, and whether touch
  // actions can be performed on it.
  virtual bool HasTouchScreen() const = 0;

  // Return the page load strategy for this session.
  virtual std::string page_load_strategy() const = 0;

  // Quits Chrome.
  virtual Status Quit() = 0;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_H_
