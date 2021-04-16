// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_NAVIGATION_TRACKER_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_NAVIGATION_TRACKER_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/page_load_strategy.h"
#include "chrome/test/chromedriver/chrome/status.h"

namespace base {
class DictionaryValue;
}

struct BrowserInfo;
class DevToolsClient;
class JavaScriptDialogManager;
class Status;
class Timeout;

// Tracks the navigation state of the page.
class NavigationTracker : public DevToolsEventListener,
                          public PageLoadStrategy {
 public:
  NavigationTracker(DevToolsClient* client,
                    WebView* web_view,
                    const BrowserInfo* browser_info,
                    const JavaScriptDialogManager* dialog_manager,
                    const bool is_eager = false);

  NavigationTracker(DevToolsClient* client,
                    LoadingState known_state,
                    WebView* web_view,
                    const BrowserInfo* browser_info,
                    const JavaScriptDialogManager* dialog_manager,
                    const bool is_eager = false);

  ~NavigationTracker() override;

  // Overridden from PageLoadStrategy:
  // Gets whether a navigation is pending for the current frame.
  Status IsPendingNavigation(const Timeout* timeout, bool* is_pending) override;
  void set_timed_out(bool timed_out) override;
  // Calling SetFrame with empty string means setting it to
  // top frame
  void SetFrame(const std::string& new_frame_id) override;
  bool IsNonBlocking() const override;

  Status CheckFunctionExists(const Timeout* timeout, bool* exists);

  // Overridden from DevToolsEventListener:
  Status OnConnected(DevToolsClient* client) override;
  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::DictionaryValue& params) override;
  Status OnCommandSuccess(DevToolsClient* client,
                          const std::string& method,
                          const base::DictionaryValue* result,
                          const Timeout& command_timeout) override;

 private:
  Status UpdateCurrentLoadingState();
  // Use for read access to loading_state_
  LoadingState loadingState();
  // Only set access loading_state_ if this is true
  bool hasCurrentFrame();
  void setCurrentFrameInvalid();
  void initCurrentFrame(LoadingState state);
  void clearFrameStates();
  DevToolsClient* client_;
  WebView* web_view_;
  const std::string top_frame_id_;
  // May be empty to signify current frame is
  // no longer valid
  std::string current_frame_id_;
  const JavaScriptDialogManager* dialog_manager_;
  const bool is_eager_;
  bool timed_out_;
  std::unordered_map<std::string, LoadingState> frame_to_state_map_;
  LoadingState* loading_state_;
  // Used when current frame is invalid
  LoadingState dummy_state_;

  DISALLOW_COPY_AND_ASSIGN(NavigationTracker);
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_NAVIGATION_TRACKER_H_
