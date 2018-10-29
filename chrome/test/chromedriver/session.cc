// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/session.h"

#include <list>
#include <utility>

#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/chrome.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/web_view.h"
#include "chrome/test/chromedriver/logging.h"

namespace {

base::LazyInstance<base::ThreadLocalPointer<Session>>::DestructorAtExit
    lazy_tls_session = LAZY_INSTANCE_INITIALIZER;

}  // namespace

FrameInfo::FrameInfo(const std::string& parent_frame_id,
                     const std::string& frame_id,
                     const std::string& chromedriver_frame_id)
    : parent_frame_id(parent_frame_id),
      frame_id(frame_id),
      chromedriver_frame_id(chromedriver_frame_id) {}

// The default timeout values came from W3C spec.
const base::TimeDelta Session::kDefaultImplicitWaitTimeout =
    base::TimeDelta::FromSeconds(0);
const base::TimeDelta Session::kDefaultPageLoadTimeout =
    base::TimeDelta::FromSeconds(300);
const base::TimeDelta Session::kDefaultScriptTimeout =
    base::TimeDelta::FromSeconds(30);

Session::Session(const std::string& id)
    : id(id),
      w3c_compliant(kW3CDefault),
      quit(false),
      detach(false),
      force_devtools_screenshot(false),
      sticky_modifiers(0),
      mouse_position(0, 0),
      pressed_mouse_button(kNoneMouseButton),
      implicit_wait(kDefaultImplicitWaitTimeout),
      page_load_timeout(kDefaultPageLoadTimeout),
      script_timeout(kDefaultScriptTimeout),
      auto_reporting_enabled(false) {}

Session::Session(const std::string& id, std::unique_ptr<Chrome> chrome)
    : id(id),
      w3c_compliant(kW3CDefault),
      quit(false),
      detach(false),
      force_devtools_screenshot(false),
      chrome(std::move(chrome)),
      sticky_modifiers(0),
      mouse_position(0, 0),
      pressed_mouse_button(kNoneMouseButton),
      implicit_wait(kDefaultImplicitWaitTimeout),
      page_load_timeout(kDefaultPageLoadTimeout),
      script_timeout(kDefaultScriptTimeout),
      auto_reporting_enabled(false) {}

Session::~Session() {}

Status Session::GetTargetWindow(WebView** web_view) {
  if (!chrome)
    return Status(kNoSuchWindow, "no chrome started in this session");

  Status status = chrome->GetWebViewById(window, web_view);
  if (status.IsError())
    status = Status(kNoSuchWindow, "target window already closed", status);
  return status;
}

void Session::SwitchToTopFrame() {
  frames.clear();
}

void Session::SwitchToParentFrame() {
  if (!frames.empty())
    frames.pop_back();
}

void Session::SwitchToSubFrame(const std::string& frame_id,
                               const std::string& chromedriver_frame_id) {
  std::string parent_frame_id;
  if (!frames.empty())
    parent_frame_id = frames.back().frame_id;
  frames.push_back(FrameInfo(parent_frame_id, frame_id, chromedriver_frame_id));
}

std::string Session::GetCurrentFrameId() const {
  if (frames.empty())
    return std::string();
  return frames.back().frame_id;
}

std::vector<WebDriverLog*> Session::GetAllLogs() const {
  std::vector<WebDriverLog*> logs;
  for (const auto& log : devtools_logs)
    logs.push_back(log.get());
  if (driver_log)
    logs.push_back(driver_log.get());
  return logs;
}

std::string Session::GetFirstBrowserError() const {
  for (const auto& log : devtools_logs) {
    if (log->type() == WebDriverLog::kBrowserType) {
      std::string message = log->GetFirstErrorMessage();
      if (!message.empty())
        return message;
    }
  }
  return std::string();
}

Session* GetThreadLocalSession() {
  return lazy_tls_session.Pointer()->Get();
}

void SetThreadLocalSession(std::unique_ptr<Session> session) {
  lazy_tls_session.Pointer()->Set(session.release());
}
