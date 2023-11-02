// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_USER_EXPERIMENT_H_
#define CHROME_INSTALLER_SETUP_USER_EXPERIMENT_H_

#include "chrome/installer/util/experiment_metrics.h"
#include "chrome/installer/util/experiment_storage.h"
#include "chrome/installer/util/util_constants.h"

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace installer {

class Experiment;
class ExperimentStorage;
class InstallationState;
class InstallerState;
class InitialPreferences;

// Returns true if a user of this Chrome install should participate in a
// post-update user experiment.
bool ShouldRunUserExperiment(const InstallerState& installer_state);

// Initiates the user experiment for a user of the current install. May only be
// called if eligibility had previously been evaluated via
// ShouldRunUserExperiment. |setup_path| is the path to the version of setup.exe
// that will be spawned to run the experiment. If |user_context| is true,
// setup.exe will be spawned directly; otherwise, it will be either be run as
// the interactive console user or on the next login via Active Setup.
void BeginUserExperiment(const InstallerState& installer_state,
                         const base::FilePath& setup_path,
                         bool user_context);

// Runs the experiment for the current user.
void RunUserExperiment(const base::CommandLine& command_line,
                       const InitialPreferences& initial_preferences,
                       InstallationState* original_state,
                       InstallerState* installer_state);

// Writes the initial state |state| to the registry if there is no existing
// state for this or another user.
void WriteInitialState(ExperimentStorage* storage,
                       ExperimentMetrics::State state);

// Returns true if the machine is joined to a Windows domain.
bool IsDomainJoined();

// Returns true if the machine is selected for participation in |current_study|.
// Dice are rolled on the first invocation to determine in which study the
// machine participates.
bool IsSelectedForStudy(ExperimentStorage::Lock* lock,
                        ExperimentStorage::Study current_study);

// Returns a group number based on the study in which the client participates.
int PickGroup(ExperimentStorage::Study participation);

// Returns true if the installed version of Chrome doesn't match the current
// executable's.
bool IsUpdateRenamePending();

// Launches Chrome to present the prompt.
void LaunchChrome(const InstallerState& installer_state,
                  const Experiment& experiment);

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_USER_EXPERIMENT_H_
