// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_OUTPUT_CONTROLLER_TEST_API_H_
#define COMPONENTS_EXO_WAYLAND_OUTPUT_CONTROLLER_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "components/exo/wayland/output_controller.h"

namespace exo::wayland {

class OutputControllerTestApi {
 public:
  explicit OutputControllerTestApi(OutputController& output_controller);
  OutputControllerTestApi(const OutputControllerTestApi&) = delete;
  OutputControllerTestApi& operator=(const OutputControllerTestApi&) = delete;
  virtual ~OutputControllerTestApi() = default;

  WaylandDisplayOutput* GetWaylandDisplayOutput(int64_t display_id);
  const OutputController::DisplayOutputMap& GetOutputMap() const;
  int64_t GetDispatchedActivatedDisplayId() const;
  void ResetDisplayManagerObservation();

 private:
  const raw_ref<OutputController> output_controller_;
};

}  // namespace exo::wayland

#endif  // COMPONENTS_EXO_WAYLAND_OUTPUT_CONTROLLER_TEST_API_H_
