// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_DOM_TRACKER_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_DOM_TRACKER_H_

#include <map>
#include <string>

#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"

namespace base {
class DictionaryValue;
class Value;
}

class DevToolsClient;
class Status;

// Tracks the state of the DOM and execution context creation.
class DomTracker : public DevToolsEventListener {
 public:
  explicit DomTracker(DevToolsClient* client);

  DomTracker(const DomTracker&) = delete;
  DomTracker& operator=(const DomTracker&) = delete;

  ~DomTracker() override;

  Status GetFrameIdForNode(int node_id, std::string* frame_id);

  // Overridden from DevToolsEventListener:
  Status OnConnected(DevToolsClient* client) override;
  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::DictionaryValue& params) override;

 private:
  bool ProcessNodeList(const base::Value& nodes);
  bool ProcessNode(const base::Value& node);
  void ProcessFencedFrameShadowDom(const base::Value& node);
  Status RebuildMapping(DevToolsClient* client);

  std::map<int, std::string> node_to_frame_map_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_DOM_TRACKER_H_
