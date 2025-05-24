// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/stub_chrome.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/web_view.h"

StubChrome::StubChrome() = default;

StubChrome::~StubChrome() = default;

Status StubChrome::GetAsDesktop(ChromeDesktopImpl** desktop) {
  return Status(kUnknownError, "not supported");
}

const BrowserInfo* StubChrome::GetBrowserInfo() const {
  return &browser_info_;
}

bool StubChrome::HasCrashedWebView() {
  return false;
}

int StubChrome::GetWebViewCount() const {
  return 1;
}

Status StubChrome::GetWebViewIdForFirstTab(std::string* web_view_id,
                                           bool w3c_compliant) {
  return Status(kOk);
}

Status StubChrome::GetTopLevelWebViewIds(std::list<std::string>* tab_view_ids,
                                         bool w3c_compliant) {
  return Status(kOk);
}

Status StubChrome::GetWebViewById(const std::string& id, WebView** web_view) {
  return Status(kOk);
}

Status StubChrome::GetActivePageByWebViewId(const std::string& id,
                                            WebView** web_view,
                                            bool wait_for_page) {
  return Status(kOk);
}

Status StubChrome::NewHiddenTarget(const std::string& target_id,
                                   bool w3c_compliant,
                                   std::string* window_handle) {
  return Status(kOk);
}

Status StubChrome::NewWindow(const std::string& target_id,
                             WindowType type,
                             bool is_background,
                             bool w3c_compliant,
                             std::string* window_handle) {
  return Status(kOk);
}

Status StubChrome::GetWindowRect(const std::string& id, WindowRect* rect) {
  return Status(kOk);
}

Status StubChrome::SetWindowRect(const std::string& target_id,
                                 const base::Value::Dict& params) {
  return Status(kOk);
}

Status StubChrome::MaximizeWindow(const std::string& target_id) {
  return Status(kOk);
}

Status StubChrome::MinimizeWindow(const std::string& target_id) {
  return Status(kOk);
}

Status StubChrome::FullScreenWindow(const std::string& target_id) {
  return Status(kOk);
}

Status StubChrome::CloseWebView(const std::string& id) {
  return Status(kOk);
}

Status StubChrome::ActivateWebView(const std::string& id) {
  return Status(kOk);
}

Status StubChrome::SetAcceptInsecureCerts() {
  return Status(kOk);
}

Status StubChrome::SetPermission(
    std::unique_ptr<base::Value::Dict> permission_descriptor,
    Chrome::PermissionState desired_state,
    WebView* current_view) {
  return Status(kOk);
}

std::string StubChrome::GetOperatingSystemName() {
  return std::string();
}

bool StubChrome::IsMobileEmulationEnabled() const {
  return false;
}

bool StubChrome::HasTouchScreen() const {
  return false;
}

std::string StubChrome::page_load_strategy() const {
  return std::string();
}

Status StubChrome::Quit() {
  return Status(kOk);
}

DevToolsClient* StubChrome::Client() const {
  return nullptr;
}
