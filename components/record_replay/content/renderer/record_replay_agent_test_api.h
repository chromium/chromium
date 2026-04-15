// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_CONTENT_RENDERER_RECORD_REPLAY_AGENT_TEST_API_H_
#define COMPONENTS_RECORD_REPLAY_CONTENT_RENDERER_RECORD_REPLAY_AGENT_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "components/record_replay/content/renderer/record_replay_agent.h"

namespace record_replay {

// A test utility class that exposes internal event emission methods of
// `RecordReplayAgent` to tests, allowing them to simulate DOM interactions
// (such as mouse downs, selection changes, and text edits) efficiently.
class RecordReplayAgentTestApi {
 public:
  explicit RecordReplayAgentTestApi(RecordReplayAgent* agent)
      : agent_(*agent) {}

  void DidReceiveLeftMouseDownOrGestureTapInNode(const blink::WebNode& node) {
    agent_->DidReceiveLeftMouseDownOrGestureTapInNode(node);
  }

  void SelectControlSelectionChanged(
      const blink::WebFormControlElement& element) {
    agent_->SelectControlSelectionChanged(element);
  }

  void TextFieldDidEndEditing(const blink::WebInputElement& element) {
    agent_->TextFieldDidEndEditing(element);
  }

 private:
  const raw_ref<RecordReplayAgent> agent_;
};

inline RecordReplayAgentTestApi test_api(RecordReplayAgent& agent) {
  return RecordReplayAgentTestApi(&agent);
}

}  // namespace record_replay

#endif  // COMPONENTS_RECORD_REPLAY_CONTENT_RENDERER_RECORD_REPLAY_AGENT_TEST_API_H_
