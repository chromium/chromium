// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_PAGE_TRACKER_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_PAGE_TRACKER_H_

#include <list>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"

class DevToolsClient;
class WebViewImpl;
class Status;

// Tracks execution context creation.
class PageTracker : public DevToolsEventListener {
 public:
  PageTracker(DevToolsClient* client,
              std::list<std::unique_ptr<WebViewImpl>>* web_views);

  PageTracker(const PageTracker&) = delete;
  PageTracker& operator=(const PageTracker&) = delete;

  ~PageTracker() override;

  bool ListensToConnections() const override;

  // Overridden from DevToolsEventListener:
  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::Value::Dict& params) override;

 private:
  raw_ptr<std::list<std::unique_ptr<WebViewImpl>>> web_views_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_PAGE_TRACKER_H_
