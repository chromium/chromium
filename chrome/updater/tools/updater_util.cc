// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants_default.h"
#include "chrome/updater/ipc/ipc_support.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/update_service.h"

namespace updater::tools {

constexpr char kProductSwitch[] = "product";
constexpr char kBackgroundSwitch[] = "background";
constexpr char kListAppsSwitch[] = "list-apps";
constexpr char kListUpdateSwitch[] = "list-update";
constexpr char kListPoliciesSwitch[] = "list-policies";
constexpr char kJSONFormatSwitch[] = "json";
constexpr char kUpdateSwitch[] = "update";

UpdaterScope Scope() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kSystemSwitch)
             ? UpdaterScope::kSystem
             : UpdaterScope::kUser;
}

UpdateService::Priority Priority() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kBackgroundSwitch)
             ? UpdateService::Priority::kBackground
             : UpdateService::Priority::kForeground;
}

bool OutputInJSONFormat() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kJSONFormatSwitch);
}

std::string Quoted(const std::string& value) {
  return "\"" + value + "\"";
}

void OnAppStateChanged(const UpdateService::UpdateState& update_state) {
  switch (update_state.state) {
    case UpdateService::UpdateState::State::kCheckingForUpdates:
      std::cout << Quoted(update_state.app_id) << ": checking update... "
                << std::endl;
      break;

    case UpdateService::UpdateState::State::kUpdateAvailable:
      std::cout << Quoted(update_state.app_id)
                << ": update found, next version = "
                << update_state.next_version << std::endl;
      break;

    case UpdateService::UpdateState::State::kDownloading:
      std::cout << Quoted(update_state.app_id)
                << ": downloading update, downloaded bytes: "
                << update_state.downloaded_bytes
                << ", total: " << update_state.total_bytes << std::endl;
      break;

    case UpdateService::UpdateState::State::kInstalling:
      std::cout << Quoted(update_state.app_id)
                << ": installing update, progress at: "
                << update_state.install_progress << std::endl;
      break;

    case UpdateService::UpdateState::State::kUpdated:
      std::cout << Quoted(update_state.app_id)
                << ": updated version = " << update_state.next_version
                << std::endl;
      break;

    case UpdateService::UpdateState::State::kNoUpdate:
      std::cout << Quoted(update_state.app_id) << ": is up-to-date."
                << std::endl;
      break;

    case UpdateService::UpdateState::State::kUpdateError:
      std::cout << Quoted(update_state.app_id) << ": update failed"
                << ", error code: " << update_state.error_code
                << ", extra code: " << update_state.extra_code1 << std::endl;
      break;

    default:
      std::cout << Quoted(update_state.app_id)
                << ": unexpected update state: " << update_state.state
                << std::endl;
      break;
  }
}

void OnUpdateComplete(base::OnceCallback<void(int)> cb,
                      UpdateService::Result result) {
  if (result == UpdateService::Result::kSuccess) {
    std::cout << "App update finished successfully." << std::endl;
    std::move(cb).Run(0);
  } else {
    std::cout << "Failed to update app(s), result = " << result << std::endl;
    std::move(cb).Run(1);
  }
}

class AppState : public base::RefCountedThreadSafe<AppState> {
 public:
  AppState(const std::string& app_id, const std::string& version)
      : app_id_(app_id), current_version_(version) {}

  std::string app_id() const { return app_id_; }
  std::string current_version() const { return current_version_; }
  std::string next_version() const { return next_version_; }
  void set_next_version(const std::string& next_version) {
    next_version_ = next_version;
  }

 protected:
  virtual ~AppState() = default;

 private:
  friend class base::RefCountedThreadSafe<AppState>;

  const std::string app_id_;
  const std::string current_version_;
  std::string next_version_;
};

class UpdaterUtilApp : public App {
 public:
  UpdaterUtilApp() : service_proxy_(CreateUpdateServiceProxy(Scope())) {}

 private:
  ~UpdaterUtilApp() override = default;
  void FirstTaskRun() override;

  void PrintUsage(const std::string& error_message);
  void ListApps();
  void ListUpdate();
  void Update();
  void ListPolicies();

  void FindApp(const std::string& app_id,
               base::OnceCallback<void(scoped_refptr<AppState>)> callback);
  void DoListUpdate(scoped_refptr<AppState> app_state);
  void DoUpdateApp(scoped_refptr<AppState> app_state);

  scoped_refptr<UpdateService> service_proxy_;
  ScopedIPCSupportWrapper ipc_support_;
};

void UpdaterUtilApp::PrintUsage(const std::string& error_message) {
  if (!error_message.empty()) {
    LOG(ERROR) << error_message;
  }

  std::cout << "Usage: "
            << base::CommandLine::ForCurrentProcess()->GetProgram().BaseName()
            << " [action...] [parameters...]" << R"(
    Actions:
        --update            Update app(s).
        --list-apps         List all registered apps.
        --list-update       List update for an app (skip update install).
        --list-policies     List all currently effective enterprise policies.
    Action parameters:
        --background        Use background priority.
        --product           ProductID.
        --system            Use the system scope.
        --json              Use JSON as output format where applicable.)"
            << std::endl;
  Shutdown(error_message.empty() ? 0 : 1);
}

void UpdaterUtilApp::ListApps() {
  service_proxy_->GetAppStates(base::BindOnce(
      [](base::OnceCallback<void(int)> cb,
         const std::vector<updater::UpdateService::AppState>& states) {
        std::cout << "Registered apps : {" << std::endl;
        for (updater::UpdateService::AppState app : states) {
          std::cout << "\t" << Quoted(app.app_id) << " = "
                    << Quoted(app.version.GetString()) << ';' << std::endl;
        }
        std::cout << '}' << std::endl;
        std::move(cb).Run(0);
      },
      base::BindOnce(&UpdaterUtilApp::Shutdown, this)));
}

void UpdaterUtilApp::FindApp(
    const std::string& app_id,
    base::OnceCallback<void(scoped_refptr<AppState>)> callback) {
  service_proxy_->GetAppStates(base::BindOnce(
      [](const std::string& app_id,
         base::OnceCallback<void(scoped_refptr<AppState>)> callback,
         const std::vector<updater::UpdateService::AppState>& states) {
        auto it = base::ranges::find_if(
            states, [&app_id](const updater::UpdateService::AppState& state) {
              return base::EqualsCaseInsensitiveASCII(state.app_id, app_id);
            });
        LOG_IF(ERROR, it == std::end(states))
            << Quoted(app_id) << " is not a registered app.";
        std::move(callback).Run(it == std::end(states)
                                    ? nullptr
                                    : base::MakeRefCounted<AppState>(
                                          app_id, it->version.GetString()));
      },
      app_id, std::move(callback)));
}

void UpdaterUtilApp::ListUpdate() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  const std::string app_id = command_line->GetSwitchValueASCII(kProductSwitch);
  if (app_id.empty()) {
    PrintUsage("Must specify a product to list update.");
    return;
  }

  FindApp(app_id, base::BindOnce(&UpdaterUtilApp::DoListUpdate, this));
}

void UpdaterUtilApp::DoListUpdate(scoped_refptr<AppState> app_state) {
  if (!app_state) {
    Shutdown(1);
    return;
  }

  service_proxy_->CheckForUpdate(
      app_state->app_id(), Priority(),
      UpdateService::PolicySameVersionUpdate::kNotAllowed,
      base::BindRepeating(
          [](scoped_refptr<AppState> app_state,
             const UpdateService::UpdateState& update_state) {
            if (update_state.state ==
                UpdateService::UpdateState::State::kUpdateAvailable) {
              app_state->set_next_version(
                  update_state.next_version.GetString());
            }
          },
          app_state),
      base::BindOnce(
          [](scoped_refptr<AppState> app_state,
             base::OnceCallback<void(int)> cb, UpdateService::Result result) {
            if (result == UpdateService::Result::kSuccess) {
              std::cout << Quoted(app_state->app_id()) << " : {" << std::endl;
              std::cout << "\tCurrent Version = "
                        << Quoted(app_state->current_version()) << ";"
                        << std::endl;
              std::cout << "\tAvailable Version = "
                        << Quoted(app_state->next_version()) << ";"
                        << std::endl;
              std::cout << "}" << std::endl;
              std::move(cb).Run(0);
            }
          },
          app_state, base::BindOnce(&UpdaterUtilApp::Shutdown, this)));
}

void UpdaterUtilApp::Update() {
  const std::string app_id =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kProductSwitch);
  if (app_id.empty()) {
    service_proxy_->UpdateAll(
        base::BindRepeating(OnAppStateChanged),
        base::BindOnce(
            [](base::OnceCallback<void(int)> cb, UpdateService::Result result) {
              OnUpdateComplete(std::move(cb), result);
            },
            base::BindOnce(&UpdaterUtilApp::Shutdown, this)));
  } else {
    FindApp(app_id, base::BindOnce(&UpdaterUtilApp::DoUpdateApp, this));
  }
}

void UpdaterUtilApp::DoUpdateApp(scoped_refptr<AppState> app_state) {
  if (!app_state) {
    Shutdown(1);
    return;
  }

  service_proxy_->Update(
      app_state->app_id(), /*install_data_index=*/"", Priority(),
      UpdateService::PolicySameVersionUpdate::kNotAllowed,
      base::BindRepeating(OnAppStateChanged),
      base::BindOnce(
          [](base::OnceCallback<void(int)> cb, UpdateService::Result result) {
            OnUpdateComplete(std::move(cb), result);
          },
          base::BindOnce(&UpdaterUtilApp::Shutdown, this)));
}

void UpdaterUtilApp::ListPolicies() {
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::WithBaseSyncPrimitives()},
      base::BindOnce([] {
        auto configurator = base::MakeRefCounted<Configurator>(
            CreateGlobalPrefs(Scope()), CreateDefaultExternalConstants());
        if (OutputInJSONFormat()) {
          std::string policy_string;
          if (base::JSONWriter::Write(
                  configurator->GetPolicyService()->GetAllPolicies(),
                  &policy_string)) {
            std::cout << policy_string << std::endl;
          } else {
            LOG(ERROR) << "Failed to write policy as JSON string.";
          }
        } else {
          std::cout
              << "Updater policies: "
              << configurator->GetPolicyService()->GetAllPoliciesAsString()
              << std::endl;
        }
      }),
      base::BindOnce(&UpdaterUtilApp::Shutdown, this, 0));
}

void UpdaterUtilApp::FirstTaskRun() {
  const std::map<std::string, void (UpdaterUtilApp::*)()> commands = {
      {kListAppsSwitch, &UpdaterUtilApp::ListApps},
      {kListUpdateSwitch, &UpdaterUtilApp::ListUpdate},
      {kUpdateSwitch, &UpdaterUtilApp::Update},
      {kListPoliciesSwitch, &UpdaterUtilApp::ListPolicies},
  };

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  for (const auto& [switch_name, func] : commands) {
    if (command_line->HasSwitch(switch_name)) {
      (this->*func)();
      return;
    }
  }

  PrintUsage("");
}

int UpdaterUtilMain(int argc, char** argv) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  updater::InitLogging(Scope());
  InitializeThreadPool("updater-util");
  const base::ScopedClosureRunner shutdown_thread_pool(
      base::BindOnce([] { base::ThreadPoolInstance::Get()->Shutdown(); }));
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  return base::MakeRefCounted<UpdaterUtilApp>()->Run();
}

}  // namespace updater::tools

int main(int argc, char** argv) {
  return updater::tools::UpdaterUtilMain(argc, argv);
}
