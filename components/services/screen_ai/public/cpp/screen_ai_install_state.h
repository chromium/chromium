// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_SCREEN_AI_PUBLIC_CPP_SCREEN_AI_INSTALL_STATE_H_
#define COMPONENTS_SERVICES_SCREEN_AI_PUBLIC_CPP_SCREEN_AI_INSTALL_STATE_H_

#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/observer_list_types.h"

class PrefService;

namespace screen_ai {

class ScreenAIInstallState {
 public:
  enum class State {
    kNotDownloaded,
    kDownloading,
    kFailed,
    kReady,
  };

  class Observer : public base::CheckedObserver {
   public:
    virtual void StateChanged(State state) {}
    virtual void DownloadProgressChanged(double progress) {}
  };

  ScreenAIInstallState();
  ScreenAIInstallState(const ScreenAIInstallState&) = delete;
  ScreenAIInstallState& operator=(const ScreenAIInstallState&) = delete;
  ~ScreenAIInstallState();

  static ScreenAIInstallState* GetInstance();

  // Returns true if the component is required. If the component is needed,
  // removes the timer to delete the component from |local_state|.
  static bool ShouldInstall(PrefService* local_state);

  // Returns true if the component is not used for long enough to be removed.
  static bool ShouldUninstall(PrefService* local_state);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool IsComponentReady();

  void SetComponentReadyForTesting();

  // Sets the component state and informs the observers.
  void SetState(State state);

  // Called by component downloaders to set download progress.
  void SetDownloadProgress(double progress);

  // Stores the path the component folder and sets the state to ready.
  void SetComponentFolder(const base::FilePath& component_folder);

  base::FilePath get_component_binary_path() { return component_binary_path_; }

  State get_state() { return state_; }

  void ResetForTesting();

 private:
  base::FilePath component_binary_path_;
  State state_ = State::kNotDownloaded;

  std::vector<Observer*> observers_;
};

}  // namespace screen_ai

#endif  // COMPONENTS_SERVICES_SCREEN_AI_PUBLIC_CPP_SCREEN_AI_INSTALL_STATE_H_
