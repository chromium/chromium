// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/user_experiment.h"

#include <windows.h>

#include <lm.h>
#include <shellapi.h>
#include <wtsapi32.h>

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process/launch.h"
#include "base/process/process_info.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version.h"
#include "chrome/install_static/install_modes.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/setup/setup_singleton.h"
#include "chrome/installer/setup/setup_util.h"
#include "chrome/installer/setup/update_active_setup_version_work_item.h"
#include "chrome/installer/util/experiment.h"
#include "chrome/installer/util/experiment_storage.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/util_constants.h"
#include "ui/base/fullscreen_win.h"

namespace installer {

namespace {

// The study currently being conducted.
constexpr ExperimentStorage::Study kCurrentStudy = ExperimentStorage::kStudyOne;

// The primary group for study number two.
constexpr int kStudyTwoGroup = 0;

// Test switches.
constexpr char kExperimentEnableForTesting[] = "experiment-enable-for-testing";
constexpr char kExperimentEnterpriseBypass[] = "experiment-enterprise-bypass";
constexpr char kExperimentParticipation[] = "experiment-participation";
constexpr char kExperimentRetryDelay[] = "experiment-retry-delay";

// Returns true if the experiment is enabled for testing.
bool IsExperimentEnabledForTesting() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kExperimentEnableForTesting);
}

// Returns true if the install originated from the MSI or if the machine is
// joined to a domain. This check can be bypassed via
// --experiment-enterprise-bypass.
bool IsEnterpriseInstall(const InstallerState& installer_state) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kExperimentEnterpriseBypass)) {
    return false;
  }
  return installer_state.is_msi() || IsDomainJoined();
}

// Returns the delay to be used between presentation retries. The default (five
// minutes) can be overidden via --experiment-retry-delay=SECONDS.
base::TimeDelta GetRetryDelay() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  base::string16 value =
      command_line->GetSwitchValueNative(kExperimentRetryDelay);
  int seconds;
  if (!value.empty() && base::StringToInt(value, &seconds))
    return base::TimeDelta::FromSeconds(seconds);
  return base::TimeDelta::FromMinutes(5);
}

// Overrides the participation value for testing if a value is provided via
// --experiment-participation=value. "value" may be "one" or "two". Any other
// value (or none at all) results in clearing the persisted state for organic
// re-evaluation.
ExperimentStorage::Study HandleParticipationOverride(
    ExperimentStorage::Study current_participation,
    ExperimentStorage::Lock* lock) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kExperimentParticipation))
    return current_participation;

  base::string16 participation_override =
      command_line->GetSwitchValueNative(kExperimentParticipation);
  ExperimentStorage::Study participation = ExperimentStorage::kNoStudySelected;
  if (participation_override == L"one")
    participation = ExperimentStorage::kStudyOne;
  else if (participation_override == L"two")
    participation = ExperimentStorage::kStudyTwo;

  if (participation != current_participation)
    lock->WriteParticipation(participation);

  return participation;
}

// This function launches setup as the currently logged-in interactive
// user, that is, the user whose logon session is attached to winsta0\default.
// It assumes that currently we are running as SYSTEM in a non-interactive
// window station.
// The function fails if there is no interactive session active, basically
// the computer is on but nobody has logged in locally.
// Remote Desktop sessions do not count as interactive sessions; running this
// method as a user logged in via remote desktop will do nothing.
bool LaunchSetupAsConsoleUser(const base::CommandLine& cmd_line) {
  DWORD console_id = ::WTSGetActiveConsoleSessionId();
  if (console_id == 0xFFFFFFFF) {
    PLOG(ERROR) << __func__ << " no session attached to the console";
    return false;
  }
  base::win::ScopedHandle user_token_handle;
  {
    HANDLE user_token;
    if (!::WTSQueryUserToken(console_id, &user_token)) {
      PLOG(ERROR) << __func__ << " failed to get user token for console_id "
                  << console_id;
      return false;
    }
    user_token_handle.Set(user_token);
  }
  base::LaunchOptions options;
  options.as_user = user_token_handle.Get();
  options.empty_desktop_name = true;
  VLOG(1) << "Spawning experiment process: " << cmd_line.GetCommandLineString();
  if (base::LaunchProcess(cmd_line, options).IsValid())
    return true;
  PLOG(ERROR) << "Failed";
  return false;
}

// Returns true if the Windows shell indicates that the machine isn't in
// presentation mode, running a full-screen D3D app, or in a quiet period.
bool MayShowNotifications() {
  QUERY_USER_NOTIFICATION_STATE state = {};
  HRESULT result = SHQueryUserNotificationState(&state);
  if (FAILED(result))
    return true;
  // Explicitly allow the acceptable states rather than the converse to be sure
  // there are no surprises should new states be introduced.
  return state == QUNS_NOT_PRESENT ||          // Locked/screensaver running.
         state == QUNS_ACCEPTS_NOTIFICATIONS;  // Go for it!
}

bool UserSessionIsNotYoung() {
  static constexpr base::TimeDelta kMinSessionLength =
      base::TimeDelta::FromMinutes(5);
  base::Time session_start_time = GetConsoleSessionStartTime();
  if (session_start_time.is_null())
    return true;

  base::TimeDelta session_length = base::Time::Now() - session_start_time;
  return session_length >= kMinSessionLength;
}

bool ActiveWindowIsNotFullscreen() {
  return !ui::IsFullScreenMode();
}

// Blocks processing if conditions are not right for presentation. Returns true
// if presentation should continue, or false otherwise (e.g., another process
// requires the setup singleton).
bool WaitForPresentation(
    const SetupSingleton& setup_singleton,
    Experiment* experiment,
    ExperimentStorage* storage,
    std::unique_ptr<ExperimentStorage::Lock>* storage_lock) {
  base::TimeDelta retry_delay = GetRetryDelay();
  bool first_sleep = true;
  bool loop_again = true;

  do {
    if (MayShowNotifications() && UserSessionIsNotYoung() &&
        ActiveWindowIsNotFullscreen()) {
      return true;
    }

    // Update the state accordingly if this is the first sleep.
    if (first_sleep) {
      experiment->SetState(ExperimentMetrics::kDeferringPresentation);
      (*storage_lock)->StoreExperiment(*experiment);
      first_sleep = false;
    }

    // Release the storage lock and wait on the singleton for five minutes.
    storage_lock->reset();
    // Break when another process needs the singleton.
    loop_again = !setup_singleton.WaitForInterrupt(retry_delay);
    *storage_lock = storage->AcquireLock();
  } while (loop_again);

  return false;
}

}  // namespace

// Execution may be in the context of the system or a user on it, and no
// guarantee is made regarding the setup singleton.
bool ShouldRunUserExperiment(const InstallerState& installer_state) {
  if (!install_static::kUseGoogleUpdateIntegration)
    return false;

  if (!install_static::SupportsRetentionExperiments())
    return false;

  // The current experiment only applies to Windows 10 and newer.
  if (base::win::GetVersion() < base::win::Version::WIN10)
    return false;

  // Installs originating from the MSI and domain joined machines are excluded.
  if (IsEnterpriseInstall(installer_state))
    return false;

  // Gain exclusive access to the persistent experiment state. Only per-install
  // state may be queried (participation and metrics are okay; Experiment itself
  // is not).
  ExperimentStorage storage;
  auto lock = storage.AcquireLock();

  // Bail out if this install is not selected into the fraction participating in
  // the current study.
  // NOTE: No clients will participate while this feature is under development.
  if (!IsExperimentEnabledForTesting() ||
      !IsSelectedForStudy(lock.get(), kCurrentStudy)) {
    return false;
  }

  // Skip the experiment if a user on the machine has already reached a terminal
  // state.
  ExperimentMetrics metrics;
  if (!lock->LoadMetrics(&metrics) || metrics.InTerminalState())
    return false;

  return true;
}

// Execution is from the context of the installer immediately following a
// successful update. The setup singleton is held.
void BeginUserExperiment(const InstallerState& installer_state,
                         const base::FilePath& setup_path,
                         bool user_context) {
  ExperimentStorage storage;

  // Prepare a command line to relaunch the installed setup.exe for the
  // experiment.
  base::CommandLine setup_command(setup_path);
  InstallUtil::AppendModeSwitch(&setup_command);
  if (installer_state.system_install())
    setup_command.AppendSwitch(switches::kSystemLevel);
  if (installer_state.verbose_logging())
    setup_command.AppendSwitch(switches::kVerboseLogging);
  setup_command.AppendSwitch(switches::kUserExperiment);
  // Copy any test switches used by the spawned process.
  static constexpr const char* kSwitchesToCopy[] = {
      kExperimentRetryDelay,
  };
  setup_command.CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                                 kSwitchesToCopy, base::size(kSwitchesToCopy));

  if (user_context) {
    // This is either a per-user install or a per-machine install run via
    // Active Setup as a normal user.
    DCHECK(!installer_state.system_install() ||
           base::GetCurrentProcessIntegrityLevel() == base::MEDIUM_INTEGRITY);
    VLOG(1) << "Spawning experiment process: "
            << setup_command.GetCommandLineString();
    // The installer is already running in the context of an ordinary user.
    // Relaunch directly to run the experiment.
    base::LaunchOptions launch_options;
    launch_options.force_breakaway_from_job_ = true;
    if (!base::LaunchProcess(setup_command, launch_options).IsValid()) {
      LOG(ERROR) << __func__
                 << " failed to relaunch installer for user experiment,";
      WriteInitialState(&storage, ExperimentMetrics::kRelaunchFailed);
    }
    return;
  }

  // The installer is running at high integrity, likely as SYSTEM. Relaunch as
  // the console user at medium integrity.
  VLOG(1) << "Attempting to spawn experiment as console user.";
  if (LaunchSetupAsConsoleUser(setup_command)) {
    return;
  }

  // Trigger Active Setup to run on the next user logon if this machine has
  // never participated in the experiment. This will be done at most once per
  // machine, even across updates. Doing so more often risks having excessive
  // active setup activity for some users.
  auto storage_lock = storage.AcquireLock();
  ExperimentMetrics experiment_metrics;
  if (storage_lock->LoadMetrics(&experiment_metrics) &&
      experiment_metrics.state == ExperimentMetrics::kUninitialized) {
    UpdateActiveSetupVersionWorkItem item(
        install_static::GetActiveSetupPath(),
        UpdateActiveSetupVersionWorkItem::UPDATE_AND_BUMP_SELECTIVE_TRIGGER);
    if (item.Do()) {
      VLOG(1) << "Bumped Active Setup Version for user experiment";
      experiment_metrics.state = ExperimentMetrics::kWaitingForUserLogon;
      storage_lock->StoreMetrics(experiment_metrics);
    } else {
      LOG(ERROR) << "Failed to bump Active Setup Version for user experiment.";
    }
  }
}

// This function executes within the context of a signed-in user, even for the
// case of system-level installs. In particular, it may be run in a spawned
// setup.exe immediately after a successful update or following user logon as a
// result of Active Setup.
void RunUserExperiment(const base::CommandLine& command_line,
                       const MasterPreferences& master_preferences,
                       InstallationState* original_state,
                       InstallerState* installer_state) {
  VLOG(1) << __func__;

  ExperimentStorage storage;
  std::unique_ptr<SetupSingleton> setup_singleton(SetupSingleton::Acquire(
      command_line, master_preferences, original_state, installer_state));
  if (!setup_singleton) {
    VLOG(1) << "Timed out while waiting for setup singleton";
    WriteInitialState(&storage, ExperimentMetrics::kSingletonWaitTimeout);
    return;
  }

  Experiment experiment;
  auto storage_lock = storage.AcquireLock();

  // Determine the study in which this client participates.
  ExperimentStorage::Study participation = ExperimentStorage::kNoStudySelected;
  if (!storage_lock->ReadParticipation(&participation) ||
      participation == ExperimentStorage::kNoStudySelected) {
    // ShouldRunUserExperiment should have brought this client into a particular
    // study. Something is very wrong if it can't be determined here.
    LOG(ERROR) << "Failed to read study participation.";
    return;
  }

  if (!storage_lock->LoadExperiment(&experiment)) {
    // The metrics correspond to a different user on the machine; nothing to do.
    VLOG(1) << "Another user is participating in the experiment.";
    return;
  }

  // There is nothing to do if the user has already reached a terminal state.
  if (experiment.metrics().InTerminalState()) {
    VLOG(1) << "Experiment has reached terminal state: "
            << experiment.metrics().state;
    return;
  }

  // Now the fun begins. Assign this user to a group if this is the first time
  // through.
  if (experiment.metrics().InInitialState()) {
    experiment.AssignGroup(PickGroup(participation));
    VLOG(1) << "Assigned user to experiment group: "
            << experiment.metrics().group;
  }

  // Chrome is considered actively used if it has been run within the last 28
  // days or if it was installed within the same time period.
  int inactive_days = GoogleUpdateSettings::GetLastRunTime();
  if (inactive_days < 0)
    inactive_days = GetInstallAge(*installer_state);
  if (inactive_days <= 28) {
    VLOG(1) << "Aborting experiment due to activity.";
    experiment.SetState(ExperimentMetrics::kIsActive);
    storage_lock->StoreExperiment(experiment);
    return;
  }
  experiment.SetInactiveDays(inactive_days);

  // Check for a dormant user: one for which the machine has been off or the
  // user has been signed out for more than 90% of the last 28 days.
  if (false) {  // TODO(grt): Implement this.
    VLOG(1) << "Aborting experiment due to dormancy.";
    experiment.SetState(ExperimentMetrics::kIsDormant);
    storage_lock->StoreExperiment(experiment);
    return;
  }

  // Note that the following call will not detect Win10 Tablet mode when
  // run under a debugger - GetForegroundWindow gets confused.
  if (base::win::IsTabletDevice(nullptr, ::GetForegroundWindow())) {
    VLOG(1) << "Aborting experiment due to tablet device.";
    experiment.SetState(ExperimentMetrics::kIsTabletDevice);
    storage_lock->StoreExperiment(experiment);
    return;
  }

  if (IsUpdateRenamePending()) {
    VLOG(1) << "Aborting experiment due to Chrome update rename pending.";
    experiment.SetState(ExperimentMetrics::kIsUpdateRenamePending);
    storage_lock->StoreExperiment(experiment);
    return;
  }

  if (!WaitForPresentation(*setup_singleton, &experiment, &storage,
                           &storage_lock)) {
    VLOG(1) << "Aborting experiment while waiting to present.";
    experiment.SetState(ExperimentMetrics::kDeferredPresentationAborted);
    storage_lock->StoreExperiment(experiment);
    return;
  }

  if (experiment.group() != ExperimentMetrics::kHoldbackGroup) {
    VLOG(1) << "Launching Chrome to show the toast.";
    experiment.SetState(ExperimentMetrics::kLaunchingChrome);
    storage_lock->StoreExperiment(experiment);
    LaunchChrome(*installer_state, experiment);
  } else {
    // Move clients in the holdback group directly into the "SelectedClose"
    // group since they will not be prompted and Chrome will not launch.
    VLOG(1) << "Skipping Chrome launch for client in the holdback group.";
    experiment.SetState(ExperimentMetrics::kSelectedClose);
    storage_lock->StoreExperiment(experiment);
  }
}

// Writes the initial state |state| to the registry if there is no existing
// state for this or another user.
void WriteInitialState(ExperimentStorage* storage,
                       ExperimentMetrics::State state) {
  auto storage_lock = storage->AcquireLock();

  // Write the state provided that there is either no existing state or that
  // any that exists also represents an initial state.
  ExperimentMetrics experiment_metrics;
  if (storage_lock->LoadMetrics(&experiment_metrics) &&
      experiment_metrics.InInitialState()) {
    experiment_metrics.state = state;
    storage_lock->StoreMetrics(experiment_metrics);
  }
}

bool IsDomainJoined() {
  LPWSTR buffer = nullptr;
  NETSETUP_JOIN_STATUS buffer_type = NetSetupUnknownStatus;
  bool is_joined =
      ::NetGetJoinInformation(nullptr, &buffer, &buffer_type) == NERR_Success &&
      buffer_type == NetSetupDomainName;
  if (buffer)
    NetApiBufferFree(buffer);

  return is_joined;
}

bool IsSelectedForStudy(ExperimentStorage::Lock* lock,
                        ExperimentStorage::Study current_study) {
  ExperimentStorage::Study participation = ExperimentStorage::kNoStudySelected;
  if (!lock->ReadParticipation(&participation))
    return false;

  participation = HandleParticipationOverride(participation, lock);

  if (participation == ExperimentStorage::kNoStudySelected) {
    // This install has not been evaluated for participation. Do so now. Select
    // a small population into the first study of the experiment. Everyone else
    // gets the second study.
    participation = base::RandDouble() <= 0.02756962532
                        ? ExperimentStorage::kStudyOne
                        : ExperimentStorage::kStudyTwo;
    if (!lock->WriteParticipation(participation))
      return false;
  }

  return participation <= current_study;
}

int PickGroup(ExperimentStorage::Study participation) {
  DCHECK(participation == ExperimentStorage::kStudyOne ||
         participation == ExperimentStorage::kStudyTwo);
  if (participation == ExperimentStorage::kStudyOne) {
    // Evenly distrubute clients among the groups.
    return base::RandInt(0, ExperimentMetrics::kNumGroups - 1);
  }

  // 1% holdback, 99% in the winning group.
  return base::RandDouble() < 0.01 ? ExperimentMetrics::kHoldbackGroup
                                   : kStudyTwoGroup;
}

bool IsUpdateRenamePending() {
  // Consider an update to be pending if an "opv" value is present in the
  // registry or if Chrome's version as registered with Omaha doesn't match the
  // current version.
  base::string16 clients_key_path =
      install_static::GetClientsKeyPath(install_static::GetAppGuid());
  const HKEY root = install_static::IsSystemInstall() ? HKEY_LOCAL_MACHINE
                                                      : HKEY_CURRENT_USER;
  base::win::RegKey clients_key;

  // The failure modes below are generally indicitive of a run from a
  // non-installed Chrome. Pretend that all is well.
  if (clients_key.Open(root, clients_key_path.c_str(),
                       KEY_WOW64_64KEY | KEY_QUERY_VALUE) != ERROR_SUCCESS) {
    return false;
  }
  if (clients_key.HasValue(google_update::kRegOldVersionField))
    return true;
  base::string16 product_version;
  if (clients_key.ReadValue(google_update::kRegVersionField,
                            &product_version) != ERROR_SUCCESS) {
    return false;
  }
  return product_version != TEXT(CHROME_VERSION_STRING);
}

void LaunchChrome(const InstallerState& installer_state,
                  const Experiment& experiment) {
  const base::FilePath chrome_exe =
      installer_state.target_path().Append(kChromeExe);
  base::CommandLine command_line(chrome_exe);
#if defined(OS_WIN)
  command_line.AppendSwitchNative(::switches::kTryChromeAgain,
                                  base::NumberToString16(experiment.group()));
#endif  // defined(OS_WIN)

  STARTUPINFOW startup_info = {sizeof(startup_info)};
  PROCESS_INFORMATION temp_process_info = {};
  base::string16 writable_command_line_string(
      command_line.GetCommandLineString());
  if (!::CreateProcess(
          chrome_exe.value().c_str(), &writable_command_line_string[0],
          nullptr /* lpProcessAttributes */, nullptr /* lpThreadAttributes */,
          FALSE /* bInheritHandles */, CREATE_NO_WINDOW,
          nullptr /* lpEnvironment */, nullptr /* lpCurrentDirectory */,
          &startup_info, &temp_process_info)) {
    PLOG(ERROR) << "Failed to launch: " << writable_command_line_string;
    return;
  }

  // Ensure that the thread and process handles of the new process are closed.
  base::win::ScopedProcessInformation process_info(temp_process_info);
}

}  // namespace installer
