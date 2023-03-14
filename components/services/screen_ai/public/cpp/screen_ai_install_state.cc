// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/public/cpp/screen_ai_install_state.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/services/screen_ai/public/cpp/pref_names.h"
#include "components/services/screen_ai/public/cpp/utilities.h"
#include "ui/accessibility/accessibility_features.h"

#if BUILDFLAG(IS_LINUX)
#include "base/cpu.h"
#endif

namespace {
const int kScreenAICleanUpDelayInDays = 30;
const char kMinExpectedVersion[] = "112.3";
}

namespace {

bool IsDeviceCompatible() {
  // Check if the CPU has the required instruction set to run the Screen AI
  // library.
#if BUILDFLAG(IS_LINUX)
  if (!base::CPU().has_sse41()) {
    return false;
  }
#endif
  return true;
}

}  // namespace

namespace screen_ai {

// static
ScreenAIInstallState* ScreenAIInstallState::GetInstance() {
  static base::NoDestructor<ScreenAIInstallState> instance;
  return instance.get();
}

// static
bool ScreenAIInstallState::VerifyLibraryVersion(const std::string& version) {
  if (version >= kMinExpectedVersion) {
    return true;
  }
  VLOG(0) << "Screen AI library version is expected to be at least "
          << kMinExpectedVersion << ", but it is: " << version;
  return false;
}

ScreenAIInstallState::ScreenAIInstallState() = default;
ScreenAIInstallState::~ScreenAIInstallState() = default;

// static
bool ScreenAIInstallState::ShouldInstall(PrefService* local_state) {
  if (!features::IsScreenAIServiceNeeded() || !IsDeviceCompatible()) {
    return false;
  }

  // Remove scheduled time for deletion as feature is needed.
  local_state->SetTime(prefs::kScreenAIScheduledDeletionTimePrefName,
                       base::Time());
  return true;
}

// static
bool ScreenAIInstallState::ShouldUninstall(PrefService* local_state) {
  if (features::IsScreenAIServiceNeeded())
    return false;

  base::Time deletion_time =
      local_state->GetTime(prefs::kScreenAIScheduledDeletionTimePrefName);

  // Set deletion time if it is not set yet.
  if (deletion_time.is_null()) {
    local_state->SetTime(
        prefs::kScreenAIScheduledDeletionTimePrefName,
        base::Time::Now() + base::Days(kScreenAICleanUpDelayInDays));
    return false;
  }

  return deletion_time >= base::Time::Now();
}

void ScreenAIInstallState::AddObserver(
    ScreenAIInstallState::Observer* observer) {
  observers_.push_back(observer);
  observer->StateChanged(state_);

  // Adding an observer indicates that we need the component.
  if (state_ == State::kNotDownloaded) {
    DownloadComponent();
  }
}

void ScreenAIInstallState::RemoveObserver(
    ScreenAIInstallState::Observer* observer) {
  auto pos = base::ranges::find(observers_, observer);
  if (pos != observers_.end())
    observers_.erase(pos);
}

void ScreenAIInstallState::SetComponentFolder(
    const base::FilePath& component_folder) {
  component_binary_path_ =
      component_folder.Append(GetComponentBinaryFileName());

  // A new component may be downloaded when an older version already exists and
  // is ready to use. We don't need to set the state again and call the
  // observers to tell this. If the older component is already in use, current
  // session will continue using that and the new one will be used after next
  // Chrome restart. Otherwise the new component will be used when a service
  // request arrives as its path is stored in |component_binary_path_|.
  if (state_ != State::kReady && state_ != State::kDownloaded) {
    SetState(State::kDownloaded);
  }
}

void ScreenAIInstallState::SetState(State state) {
  if (state == state_) {
    // Failed and ready state can be repeated as they come from different
    // profiles. The other state changes are controlled by singluar objects.
    // TODO(crbug.com/1278249): While the case is highly unexpected, add more
    // control logic if state is changed from failed to ready or vice versa.
    DCHECK(state == State::kReady || state == State::kFailed);
    return;
  }

  state_ = state;
  for (ScreenAIInstallState::Observer* observer : observers_)
    observer->StateChanged(state_);
}

void ScreenAIInstallState::DownloadComponent() {
  // TODO(crbug.com/1278249): Actually trigger download. Download is now
  // triggered on browser start based on enabled flags.
}

void ScreenAIInstallState::SetDownloadProgress(double progress) {
  DCHECK_EQ(state_, State::kDownloading);
  for (ScreenAIInstallState::Observer* observer : observers_)
    observer->DownloadProgressChanged(progress);
}

bool ScreenAIInstallState::IsComponentAvailable() {
  return !get_component_binary_path().empty();
}

void ScreenAIInstallState::SetComponentReadyForTesting() {
  state_ = State::kReady;
}

void ScreenAIInstallState::ResetForTesting() {
  state_ = State::kNotDownloaded;
  component_binary_path_.clear();
}

}  // namespace screen_ai
