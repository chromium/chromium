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

namespace {
const int kScreenAICleanUpDelayInDays = 30;
}

namespace screen_ai {

// static
ScreenAIInstallState* ScreenAIInstallState::GetInstance() {
  static base::NoDestructor<ScreenAIInstallState> instance;
  return instance.get();
}

ScreenAIInstallState::ScreenAIInstallState() = default;
ScreenAIInstallState::~ScreenAIInstallState() = default;

// static
bool ScreenAIInstallState::ShouldInstall(PrefService* local_state) {
  if (!features::IsScreenAIServiceNeeded())
    return false;

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

  SetState(State::kReady);
}

void ScreenAIInstallState::SetState(State state) {
  DCHECK_NE(state_, state);
  state_ = state;
  for (ScreenAIInstallState::Observer* observer : observers_)
    observer->StateChanged(state_);
}

void ScreenAIInstallState::SetDownloadProgress(double progress) {
  DCHECK_EQ(state_, State::kDownloading);
  for (ScreenAIInstallState::Observer* observer : observers_)
    observer->DownloadProgressChanged(progress);
}

bool ScreenAIInstallState::IsComponentReady() {
  return state_ == State::kReady;
}

void ScreenAIInstallState::SetComponentReadyForTesting() {
  state_ = State::kReady;
}

void ScreenAIInstallState::ResetForTesting() {
  state_ = State::kNotDownloaded;
  component_binary_path_.clear();
}

}  // namespace screen_ai
