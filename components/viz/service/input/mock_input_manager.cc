// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/input/mock_input_manager.h"

namespace viz {

MockInputManager::MockInputManager(FrameSinkManagerImpl* frame_sink_manager)
    : InputManager(frame_sink_manager) {}

MockInputManager::~MockInputManager() = default;

bool MockInputManager::RIRExistsForFrameSinkId(
    const FrameSinkId& frame_sink_id) {
  return base::Contains(rir_map_, frame_sink_id);
}

}  // namespace viz
