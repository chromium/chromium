// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_TETHER_COMPONENT_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_TETHER_COMPONENT_H_

#include "chromeos/ash/components/tether/tether_component.h"
#include "chromeos/ash/components/tether/tether_disconnector.h"

namespace ash {

namespace tether {

// Test double for TetherComponent.
class FakeTetherComponent : public TetherComponent {
 public:
  explicit FakeTetherComponent(bool has_asynchronous_shutdown);

  FakeTetherComponent(const FakeTetherComponent&) = delete;
  FakeTetherComponent& operator=(const FakeTetherComponent&) = delete;

  ~FakeTetherComponent() override;

  void set_has_asynchronous_shutdown(bool has_asynchronous_shutdown) {
    has_asynchronous_shutdown_ = has_asynchronous_shutdown;
  }

  ShutdownReason* last_shutdown_reason() { return last_shutdown_reason_.get(); }

  // Should only be called when status() == SHUTTING_DOWN.
  void FinishAsynchronousShutdown();

  // Initializer:
  void RequestShutdown(const ShutdownReason& shutdown_reason) override;

 private:
  bool has_asynchronous_shutdown_;
  std::unique_ptr<ShutdownReason> last_shutdown_reason_;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_TETHER_COMPONENT_H_
