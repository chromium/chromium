// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_CORE_BROWSER_RECORD_REPLAY_DRIVER_H_
#define COMPONENTS_RECORD_REPLAY_CORE_BROWSER_RECORD_REPLAY_DRIVER_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/types/pass_key.h"
#include "base/unguessable_token.h"
#include "components/record_replay/core/common/aliases.h"
#include "components/record_replay/core/common/element_id.h"
#include "components/record_replay/core/common/record_replay.mojom-forward.h"

namespace record_replay {

// The browser-side endpoint for communication with the renderer
// implementation of the interface (`RecordReplayAgent`).
class RecordReplayDriver {
 public:
  virtual ~RecordReplayDriver() = default;

  // Returns true if the driver's frame is active (i.e., neither
  // prerendered nor bfcached).
  virtual bool IsActive() const = 0;

  // Returns the unique identifier of the driver's frame.
  virtual base::UnguessableToken GetFrameToken() const = 0;

  // See mojom::RecordReplayAgent record_replay.mojom.
  virtual void StartRecording() = 0;
  virtual void StopRecording() = 0;
  virtual void GetElementSelector(DomNodeId dom_node_id,
                                  base::OnceCallback<void(Selector)> cb) = 0;
  virtual void GetMatchingElements(
      Selector element_selector,
      base::OnceCallback<void(std::vector<ElementId>)> cb) = 0;
  virtual void DoClick(DomNodeId dom_node_id,
                       base::OnceCallback<void(bool)> cb) = 0;
  virtual void DoPaste(DomNodeId dom_node_id,
                       FieldValue text,
                       base::OnceCallback<void(bool)> cb) = 0;
  virtual void DoSelect(DomNodeId dom_node_id,
                        FieldValue value,
                        base::OnceCallback<void(bool)> cb) = 0;

  virtual void SetRecordReplayAgentForTesting(
      mojom::RecordReplayAgent* agent) = 0;

 protected:
  base::PassKey<RecordReplayDriver> GetPassKey() {
    return base::PassKey<RecordReplayDriver>();
  }
};

}  // namespace record_replay

#endif  // COMPONENTS_RECORD_REPLAY_CORE_BROWSER_RECORD_REPLAY_DRIVER_H_
