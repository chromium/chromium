// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_FRAME_TRACKER_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_FRAME_TRACKER_H_

#include <map>
#include <string>
#include <unordered_set>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/web_view.h"

class DevToolsClient;
class Status;

// Tracks execution context creation.
class FrameTracker : public DevToolsEventListener {
 public:
  explicit FrameTracker(DevToolsClient* client, WebView* web_view = nullptr);

  FrameTracker(const FrameTracker&) = delete;
  FrameTracker& operator=(const FrameTracker&) = delete;

  ~FrameTracker() override;

  Status GetContextIdForFrame(const std::string& frame_id,
                              std::string* context_id) const;
  void SetContextIdForFrame(std::string frame_id, std::string context_id);
  WebView* GetTargetForFrame(const std::string& frame_id);
  bool IsKnownFrame(const std::string& frame_id) const;
  void DeleteTargetForFrame(const std::string& frame_id);

  // Overridden from DevToolsEventListener:
  Status OnConnected(DevToolsClient* client) override;
  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::Value::Dict& params) override;

 private:
  std::map<std::string, std::string> frame_to_context_map_;
  std::map<std::string, std::unique_ptr<WebView>> frame_to_target_map_;
  std::unordered_set<std::string> attached_frames_;
  raw_ptr<WebView> web_view_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_FRAME_TRACKER_H_
