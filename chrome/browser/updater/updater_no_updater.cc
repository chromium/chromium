// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/updater.h"

#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/version.h"
#include "base/version_info/version_info.h"
#include "chrome/updater/mojom/updater_service.mojom.h"

namespace updater {

base::Version CurrentlyInstalledVersion() {
  return version_info::GetVersion();
}

void EnsureUpdater(base::TaskPriority /*priority*/,
                   base::OnceClosure /*prompt*/,
                   base::OnceClosure complete) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(complete));
}

void SetUpSystemUpdater() {}

void CheckForUpdate(base::RepeatingCallback<void(const mojom::UpdateState&)>
                        version_updater_callback) {
  mojom::UpdateState state;
  state.state = mojom::UpdateState::State::kUpdateError;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(version_updater_callback, state));
}

void SchedulePeriodicTasks(base::RepeatingClosure prompt) {}

std::optional<mojom::UpdateState> GetLastOnDemandUpdateState() {
  return std::nullopt;
}

std::optional<mojom::AppState> GetLastKnownBrowserRegistration() {
  return std::nullopt;
}

std::optional<mojom::AppState> GetLastKnownUpdaterRegistration() {
  return std::nullopt;
}

void GetSystemUpdaterState(
    base::OnceCallback<void(const mojom::UpdaterState&)> callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), mojom::UpdaterState{}));
}

void GetUserUpdaterState(
    base::OnceCallback<void(const mojom::UpdaterState&)> callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), mojom::UpdaterState{}));
}

void GetSystemPoliciesJson(
    base::OnceCallback<void(const std::string&)> callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::string{}));
}

void GetUserPoliciesJson(
    base::OnceCallback<void(const std::string&)> callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::string{}));
}

void GetSystemUpdaterAppStates(
    base::OnceCallback<void(const std::vector<mojom::AppState>&)> callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::vector<mojom::AppState>{}));
}

void GetUserUpdaterAppStates(
    base::OnceCallback<void(const std::vector<mojom::AppState>&)> callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::vector<mojom::AppState>{}));
}

void SetActive() {}

}  // namespace updater
