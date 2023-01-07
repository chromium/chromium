// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_SCREEN_AI_PUBLIC_CPP_SCREEN_AI_INSTALL_STATE_H_
#define COMPONENTS_SERVICES_SCREEN_AI_PUBLIC_CPP_SCREEN_AI_INSTALL_STATE_H_

#include <vector>

#include "base/observer_list_types.h"

namespace component_updater {
class ScreenAIComponentInstallerPolicy;
}

namespace screen_ai {

class ScreenAIInstallStateTest;

class ScreenAIInstallState {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void ComponentReady() = 0;
  };

  ScreenAIInstallState();
  ScreenAIInstallState(const ScreenAIInstallState&) = delete;
  ScreenAIInstallState& operator=(const ScreenAIInstallState&) = delete;
  ~ScreenAIInstallState();

  static ScreenAIInstallState* GetInstance();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool is_component_ready() { return component_ready_; }

  void SetComponentReadyForTesting(bool ready) { component_ready_ = ready; }

 private:
  friend class component_updater::ScreenAIComponentInstallerPolicy;
  friend class ScreenAIInstallStateTest;

  void SetComponentReady();

  bool component_ready_ = false;

  std::vector<Observer*> observers_;
};

}  // namespace screen_ai

#endif  // COMPONENTS_SERVICES_SCREEN_AI_PUBLIC_CPP_SCREEN_AI_INSTALL_STATE_H_
