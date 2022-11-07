// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_SCREEN_AI_PUBLIC_CPP_SCREEN_AI_INSTALL_STATE_H_
#define COMPONENTS_SERVICES_SCREEN_AI_PUBLIC_CPP_SCREEN_AI_INSTALL_STATE_H_

#include <vector>

#include "base/files/file_path.h"
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

  base::FilePath get_component_binary_path() { return component_binary_path_; }

  void set_component_ready_for_testing() { component_ready_ = true; }

 private:
  friend class component_updater::ScreenAIComponentInstallerPolicy;
  friend class ScreenAIInstallStateTest;

  // Marks component ready and informs observers.
  void SetComponentReady(const base::FilePath& component_folder);

  base::FilePath component_binary_path_;
  bool component_ready_;

  std::vector<Observer*> observers_;
};

}  // namespace screen_ai

#endif  // COMPONENTS_SERVICES_SCREEN_AI_PUBLIC_CPP_SCREEN_AI_INSTALL_STATE_H_
