// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_PAGE_TRACKER_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_PAGE_TRACKER_H_

#include <list>
#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/web_view.h"
#include "chrome/test/chromedriver/chrome/web_view_info.h"

class DevToolsClient;
class Status;

// Tracks execution context creation.
class PageTracker : public DevToolsEventListener {
 public:
  explicit PageTracker(DevToolsClient* client, WebView* web_view = nullptr);
  PageTracker(const PageTracker&) = delete;
  PageTracker& operator=(const PageTracker&) = delete;

  ~PageTracker() override;

  // Overridden from DevToolsEventListener:
  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::Value::Dict& params) override;
  Status OnConnected(DevToolsClient* client) override;
  Status IsPendingActivePage(const Timeout* timeout, bool* is_pending);
  Status OnPageCreated(const std::string& session_id,
                       const std::string& target_id,
                       WebViewInfo::Type type);
  Status OnPageActivated(const std::string& target_id);
  Status OnPageDisconnected(const std::string& target_id);

  void DeletePage(const std::string& page_id);

  Status GetActivePage(WebView** web_view);

 private:
  raw_ptr<WebView> tab_view_;
  std::string active_page_;
  std::map<std::string, std::unique_ptr<WebView>> page_to_target_map_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_PAGE_TRACKER_H_
