// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_FRAME_TRACKER_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_FRAME_TRACKER_H_

#include <map>
#include <string>
#include <unordered_set>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/web_view.h"

namespace base {
class DictionaryValue;
}

struct BrowserInfo;
class DevToolsClient;
class Status;

// Tracks execution context creation.
class FrameTracker : public DevToolsEventListener {
 public:
  FrameTracker(DevToolsClient* client,
               WebView* web_view = nullptr,
               const BrowserInfo* browser_info = nullptr);
  ~FrameTracker() override;

  Status GetContextIdForFrame(const std::string& frame_id, int* context_id);
  WebView* GetTargetForFrame(const std::string& frame_id);
  bool IsKnownFrame(const std::string& frame_id) const;
  void DeleteTargetForFrame(const std::string& frame_id);

  // Overridden from DevToolsEventListener:
  Status OnConnected(DevToolsClient* client) override;
  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::DictionaryValue& params) override;

 private:
  std::map<std::string, int> frame_to_context_map_;
  std::map<std::string, std::unique_ptr<WebView>> frame_to_target_map_;
  std::unordered_set<std::string> attached_frames_;
  WebView* web_view_;

  DISALLOW_COPY_AND_ASSIGN(FrameTracker);
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_FRAME_TRACKER_H_
