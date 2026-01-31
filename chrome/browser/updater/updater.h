// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATER_UPDATER_H_
#define CHROME_BROWSER_UPDATER_UPDATER_H_

#include <cstdint>
#include <optional>

#include "base/functional/callback_forward.h"
#include "chrome/updater/mojom/updater_service.mojom.h"

namespace base {
enum class TaskPriority : uint8_t;
class Version;
}  // namespace base

// This module contains a number of functions to interact with the browser's
// updater (ChromiumUpdater) on Windows / macOS / Linux.
namespace updater {

// Triggers an on-demand update, reporting status updates to the callback. Must
// be called on a sequenced task runner. `callback` will be
// run on the same sequence with status updates.
void CheckForUpdate(
    base::RepeatingCallback<void(const mojom::UpdateState&)> callback);

// Get the current installed version of the browser, according to the updater.
// May block.
base::Version CurrentlyInstalledVersion();

// If this build should integrate with an updater, makes sure that an updater
// is installed and that the browser is registered with it for updates. Must be
// called on a sequenced task runner. In cases where user intervention is
// necessary, calls `prompt` (on the same sequence). After the updater is made
// present (or cannot be made present), calls `complete` on the same sequence.
void EnsureUpdater(base::TaskPriority priority,
                   base::OnceClosure prompt,
                   base::OnceClosure complete);

// Sets up a system-level updater, if possible. May prompt the user to enter
// credentials.
void SetUpSystemUpdater();

// Schedule updater periodic tasks to run five minutes later and every five
// hours thereafter. This is a backup scheduler so that even if the updater's
// scheduler is broken or disabled, it will run tasks while Chrome is running.
// Must be called on a sequenced task runner. If user intervention is needed,
// calls `prompt` on the same sequence.
void SchedulePeriodicTasks(base::RepeatingClosure prompt);

// Communicates to the updater that the browser is active.
void SetActive();

std::optional<mojom::UpdateState> GetLastOnDemandUpdateState();

std::optional<mojom::AppState> GetLastKnownBrowserRegistration();

std::optional<mojom::AppState> GetLastKnownUpdaterRegistration();

// Queries the current state of the updater. Must be called on the program's
// main sequence.
void GetSystemUpdaterState(
    base::OnceCallback<void(const mojom::UpdaterState&)> callback);
void GetUserUpdaterState(
    base::OnceCallback<void(const mojom::UpdaterState&)> callback);

// Gets the enterprise policies for the updater as a JSON blob. Must be called
// on the program's main sequence.
void GetSystemPoliciesJson(
    base::OnceCallback<void(const std::string&)> callback);
void GetUserPoliciesJson(base::OnceCallback<void(const std::string&)> callback);

// Queries the per-application metadata maintained by the updater.
void GetSystemUpdaterAppStates(
    base::OnceCallback<void(const std::vector<mojom::AppState>&)> callback);
void GetUserUpdaterAppStates(
    base::OnceCallback<void(const std::vector<mojom::AppState>&)> callback);

}  // namespace updater

#endif  // CHROME_BROWSER_UPDATER_UPDATER_H_
