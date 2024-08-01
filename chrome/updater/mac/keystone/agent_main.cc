// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <unistd.h>

#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <utility>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/ipc/ipc_support.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"

namespace updater {

namespace {

constexpr char kCommandProductID[] = "productIDToUpdate";
constexpr char kCommandPrintResults[] = "printResults";
constexpr char kCommandUserInitiated[] = "userInitiated";

// base::CommandLine can't be used because it is case-insensitive, and it does
// not support long switches name prefixed with single '-'. This argument parser
// converts an argv set into a map of switch name to switch value; for example
//    `program_name -productIDToUpdate com.google.chrome -printResults YES`
// is converted to:
//    `{"productIDToUpdate": "com.google.chrome", "printResults": "YES"}`.
std::map<std::string, std::string> ParseCommandLine(int argc,
                                                    const char* argv[]) {
  std::map<std::string, std::string> result;
  std::string key;
  for (int i = 1; i < argc; ++i) {
    std::string arg(argv[i]);
    if (base::StartsWith(arg, "-")) {
      key = arg.substr(1);
      result[key] = "";
    } else {
      if (!key.empty()) {
        result[key] = arg;
      }
      key = "";
    }
  }
  return result;
}

UpdaterScope Scope() {
  return geteuid() == 0 ? UpdaterScope::kSystem : UpdaterScope::kUser;
}

class KSAgentApp : public App {
 public:
  explicit KSAgentApp(const std::map<std::string, std::string>& switches)
      : switches_(switches) {}

 private:
  ~KSAgentApp() override = default;
  void FirstTaskRun() override;

  void ChooseServiceForApp(
      const std::string& app_id,
      base::OnceCallback<void(UpdaterScope scope)> callback) const;

  bool HasSwitch(const std::string& arg) const;
  std::string SwitchValue(const std::string& arg) const;

  void UpdateApp(const std::string& app_id);
  void DoUpdate(const std::string& app_id, UpdaterScope scope);
  void RecordUpdateResult(const UpdateService::UpdateState& update_state);
  void PrintUpdateResultAndShutDown(UpdateService::Result result);

  void Wake();

  const std::map<std::string, std::string> switches_;
  scoped_refptr<UpdateService> system_service_proxy_ =
      CreateUpdateServiceProxy(UpdaterScope::kSystem);
  scoped_refptr<UpdateService> user_service_proxy_ =
      CreateUpdateServiceProxy(UpdaterScope::kUser);
  bool update_successful_ = true;  // True when all updates succeed.
  int successful_install_count_ = 0;
  ScopedIPCSupportWrapper ipc_support_;
};

void KSAgentApp::ChooseServiceForApp(
    const std::string& app_id,
    base::OnceCallback<void(UpdaterScope)> callback) const {
  // Choose system scope if the app is a system app, otherwise choose
  // user scope.
  system_service_proxy_->GetAppStates(base::BindOnce(
      [](const std::string& app_id,
         base::OnceCallback<void(UpdaterScope)> callback,
         const std::vector<updater::UpdateService::AppState>& states) {
        std::move(callback).Run(
            base::ranges::find_if(
                states,
                [&app_id](const updater::UpdateService::AppState& state) {
                  return base::EqualsCaseInsensitiveASCII(state.app_id, app_id);
                }) == std::end(states)
                ? UpdaterScope::kUser
                : UpdaterScope::kSystem);
      },
      app_id, std::move(callback)));
}

bool KSAgentApp::HasSwitch(const std::string& arg) const {
  return switches_.contains(arg);
}

std::string KSAgentApp::SwitchValue(const std::string& arg) const {
  return HasSwitch(arg) ? switches_.at(arg) : std::string();
}

void KSAgentApp::RecordUpdateResult(
    const UpdateService::UpdateState& update_state) {
  switch (update_state.state) {
    case UpdateService::UpdateState::State::kUpdated:
      // An accurate number of successful installations is not needed.
      // A positive integer is enough to indicate that some updates are
      // installed.
      VLOG(0) << "An app update is installed successfully.";
      successful_install_count_ += 1;
      break;
    case UpdateService::UpdateState::State::kUpdateError:
      VLOG(1) << "Update error: " << update_state.error_code
              << ", extra_code1: " << update_state.extra_code1;
      update_successful_ = false;
      break;
    default:
      break;
  }
}

void KSAgentApp::PrintUpdateResultAndShutDown(UpdateService::Result result) {
  // The output string format need to be strictly followed because this is the
  // contract between the registration framework and the agent. The registration
  // framework parses the agent outputs and broadcasts the parsed results via
  // macOS notification center. Apps (like Chrome) can monitor the notification
  // center and update the UI accordingly.
  const bool update_ok =
      result == UpdateService::Result::kSuccess && update_successful_;
  std::cout << "updateCheckSuccessful_=" << (update_ok ? "YES" : "NO")
            << std::endl
            << "successfulInstallCount_=" << successful_install_count_
            << std::endl;

  Shutdown(update_ok ? 0 : 1);
}

void KSAgentApp::UpdateApp(const std::string& app_id) {
  ChooseServiceForApp(app_id,
                      base::BindOnce(&KSAgentApp::DoUpdate, this, app_id));
}

void KSAgentApp::DoUpdate(const std::string& app_id, UpdaterScope scope) {
  VLOG(0) << "Updating " << app_id << " at "
          << (scope == UpdaterScope::kSystem ? "system" : "user") << " scope.";
  scoped_refptr<UpdateService> service_proxy = scope == UpdaterScope::kSystem
                                                   ? system_service_proxy_
                                                   : user_service_proxy_;
  service_proxy->Update(
      app_id, "", UpdateService::Priority::kForeground,
      UpdateService::PolicySameVersionUpdate::kNotAllowed,
      base::BindRepeating(&KSAgentApp::RecordUpdateResult, this),
      base::BindOnce(&KSAgentApp::PrintUpdateResultAndShutDown, this));
}

void KSAgentApp::Wake() {
  for (UpdaterScope scope : {UpdaterScope::kSystem, UpdaterScope::kUser}) {
    std::optional<base::FilePath> path = GetUpdaterExecutablePath(scope);
    if (!path) {
      continue;
    }
    base::CommandLine command(*path);
    command.AppendSwitch(kWakeAllSwitch);
    if (scope == UpdaterScope::kSystem) {
      command.AppendSwitch(kSystemSwitch);
    }
    VLOG(0) << "Launching " << command.GetCommandLineString();
    base::Process process = base::LaunchProcess(command, {});
    if (process.IsValid()) {
      VLOG(0) << "Launched " << process.Pid();
    }
  }
  Shutdown(0);
}

void KSAgentApp::FirstTaskRun() {
  // The agent is a shim to trick the keystone registration framework.
  if (!SwitchValue(kCommandProductID).empty() &&
      SwitchValue(kCommandPrintResults) == "YES" &&
      SwitchValue(kCommandUserInitiated) == "YES") {
    // If the agent is run with explicit arguments to print the update result,
    // make a call directly into the service to get update details.
    UpdateApp(SwitchValue(kCommandProductID));
  } else {
    // Otherwise, just launch the --wake task. Not all callers correctly
    // provide a scope, so it will wake both scopes (if present).
    Wake();
  }
}

int KSAgentAppMain(int argc, const char* argv[]) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  updater::InitLogging(Scope());
  InitializeThreadPool("keystone");
  const base::ScopedClosureRunner shutdown_thread_pool(
      base::BindOnce([] { base::ThreadPoolInstance::Get()->Shutdown(); }));
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);

  return base::MakeRefCounted<KSAgentApp>(ParseCommandLine(argc, argv))->Run();
}

}  // namespace

}  // namespace updater

int main(int argc, const char* argv[]) {
  return updater::KSAgentAppMain(argc, argv);
}
