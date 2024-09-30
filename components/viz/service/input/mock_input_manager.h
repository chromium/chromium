// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_INPUT_MOCK_INPUT_MANAGER_H_
#define COMPONENTS_VIZ_SERVICE_INPUT_MOCK_INPUT_MANAGER_H_

#include "components/viz/service/input/input_manager.h"

namespace viz {

class FrameSinkManagerImpl;

class MockInputManager : public InputManager {
 public:
  using InputManager::rir_map_;

  explicit MockInputManager(FrameSinkManagerImpl* frame_sink_manager);

  MockInputManager(const MockInputManager&) = delete;
  MockInputManager& operator=(const MockInputManager&) = delete;

  ~MockInputManager() override;

  // Checks if a RenderInputRouter exists for |frame_sink_id|.
  bool RIRExistsForFrameSinkId(const FrameSinkId& frame_sink_id);
  int GetRenderInputRouterMapSize() { return rir_map_.size(); }

  int GetInputEventRouterMapSize() { return rwhier_map_.size(); }
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_INPUT_MOCK_INPUT_MANAGER_H_
