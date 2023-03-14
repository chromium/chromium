// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_SCREEN_AI_PUBLIC_CPP_SCREEN_AI_INSTALL_STATE_H_
#define COMPONENTS_SERVICES_SCREEN_AI_PUBLIC_CPP_SCREEN_AI_INSTALL_STATE_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/observer_list_types.h"

class PrefService;

namespace screen_ai {

class ScreenAIInstallState {
 public:
  enum class State {
    // Component does not exist on device.
    kNotDownloaded,
    // Component download is in progress.
    kDownloading,
    // Either component download or initialization failed. Component load and
    // initialization may fail due to different OS or malware protection
    // restrictions, however this is expected to be quite rare.
    // Note that if library load and initialization crashes, the Failed state
    // may never be set.
    kFailed,
    // Component is downloaded but not loaded yet.
    kDownloaded,
    // Component is initialized successfully by at least one profile.
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

  // Verifies that the library version is compatible with current Chromium
  // version. Will be used to avoid accepting the library if a newer version is
  // expected.
  static bool VerifyLibraryVersion(const std::string& version);

  // Returns true if the component is required. If the component is needed,
  // removes the timer to delete the component from |local_state|.
  static bool ShouldInstall(PrefService* local_state);

  // Returns true if the component is not used for long enough to be removed.
  static bool ShouldUninstall(PrefService* local_state);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns true if the component is downloaded and not failed to initialize.
  bool IsComponentAvailable();

  void SetComponentReadyForTesting();

  // Sets the component state and informs the observers.
  void SetState(State state);

  // Triggers component download if it's not done.
  void DownloadComponent();

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
