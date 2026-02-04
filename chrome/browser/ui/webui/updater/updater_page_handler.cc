// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/updater/updater_page_handler.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/updater/updater_ui.mojom.h"
#include "chrome/browser/updater/updater.h"
#include "chrome/enterprise_companion/global_constants.h"
#include "chrome/enterprise_companion/installer_paths.h"
#include "chrome/updater/mojom/updater_service.mojom.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"

namespace {

class DefaultUpdaterPageHandlerDelegate final
    : public UpdaterPageHandler::Delegate {
 public:
  std::optional<base::FilePath> GetUpdaterInstallDirectory(
      updater::UpdaterScope scope) const override {
    return updater::GetInstallDirectory(scope);
  }

  std::optional<base::FilePath> GetEnterpriseCompanionInstallDirectory()
      const override {
    return enterprise_companion::GetInstallDirectory();
  }

  void GetSystemUpdaterState(
      base::OnceCallback<void(const updater::mojom::UpdaterState&)> callback)
      const override {
    updater::GetSystemUpdaterState(std::move(callback));
  }

  void GetUserUpdaterState(
      base::OnceCallback<void(const updater::mojom::UpdaterState&)> callback)
      const override {
    updater::GetUserUpdaterState(std::move(callback));
  }

  void GetSystemPoliciesJson(
      base::OnceCallback<void(const std::string&)> callback) const override {
    updater::GetSystemPoliciesJson(std::move(callback));
  }

  void GetUserPoliciesJson(
      base::OnceCallback<void(const std::string&)> callback) const override {
    updater::GetUserPoliciesJson(std::move(callback));
  }

  void GetSystemUpdaterAppStates(
      base::OnceCallback<void(const std::vector<updater::mojom::AppState>&)>
          callback) const override {
    updater::GetSystemUpdaterAppStates(std::move(callback));
  }

  void GetUserUpdaterAppStates(
      base::OnceCallback<void(const std::vector<updater::mojom::AppState>&)>
          callback) const override {
    updater::GetUserUpdaterAppStates(std::move(callback));
  }

 private:
  ~DefaultUpdaterPageHandlerDelegate() override = default;
};

// Returns a vector of the per-system and per-current-user updater installation
// directories. Directories are not assumed to exist.
std::vector<base::FilePath> GetUpdaterDirectories(
    scoped_refptr<UpdaterPageHandler::Delegate> delegate) {
  std::vector<base::FilePath> paths;
  for (updater::UpdaterScope scope :
       {updater::UpdaterScope::kSystem, updater::UpdaterScope::kUser}) {
    std::optional<base::FilePath> install_path =
        delegate->GetUpdaterInstallDirectory(scope);
    if (install_path) {
      paths.push_back(*std::move(install_path));
    }
  }
  return paths;
}

// Reads an updater event log file returning the vector of newline-delimited
// messages.
std::vector<std::string> ReadUpdaterEvents(const base::FilePath& log_path) {
  if (!base::PathExists(log_path)) {
    return {};
  }
  std::string contents;
  if (!base::ReadFileToString(log_path, &contents)) {
    DPLOG(WARNING) << "Failed to read updater history log file: " << log_path;
    return {};
  }
  return base::SplitString(contents, "\n", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

struct UpdaterInfo {
  updater::mojom::UpdaterState updater_state;
  std::string policies;
};

void GetUpdaterInfo(
    scoped_refptr<UpdaterPageHandler::Delegate> delegate,
    updater::UpdaterScope scope,
    base::OnceCallback<void(const UpdaterInfo& info)> callback) {
  std::unique_ptr<UpdaterInfo> info = std::make_unique<UpdaterInfo>();
  UpdaterInfo* info_ptr = info.get();
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      2, base::BindOnce([](std::unique_ptr<UpdaterInfo> info) { return *info; },
                        std::move(info))
             .Then(std::move(callback)));

  (scope == updater::UpdaterScope::kSystem
       ? base::BindOnce(&UpdaterPageHandler::Delegate::GetSystemUpdaterState,
                        delegate)
       : base::BindOnce(&UpdaterPageHandler::Delegate::GetUserUpdaterState,
                        delegate))
      .Run(base::BindOnce(
               [](UpdaterInfo* info,
                  const updater::mojom::UpdaterState& updater_state) {
                 info->updater_state = updater_state;
               },
               info_ptr)
               .Then(barrier_closure));
  (scope == updater::UpdaterScope::kSystem
       ? base::BindOnce(&UpdaterPageHandler::Delegate::GetSystemPoliciesJson,
                        delegate)
       : base::BindOnce(&UpdaterPageHandler::Delegate::GetUserPoliciesJson,
                        delegate))
      .Run(base::BindOnce(
               [](UpdaterInfo* info, const std::string& policies) {
                 info->policies = policies;
               },
               info_ptr)
               .Then(barrier_closure));
}

updater_ui::mojom::UpdaterStatePtr ToUpdaterState(
    const UpdaterInfo& info,
    const base::FilePath& installation_directory) {
  // The client bindings for the updater return default instances in the
  // presence of errors, including when no updater is present.
  if (info.updater_state == updater::mojom::UpdaterState() ||
      info.policies.empty()) {
    return nullptr;
  }

  updater_ui::mojom::UpdaterStatePtr state =
      updater_ui::mojom::UpdaterState::New();
  state->active_version = info.updater_state.active_version;
  state->inactive_versions = info.updater_state.inactive_versions;
  if (!info.updater_state.last_checked.is_null()) {
    state->last_checked = info.updater_state.last_checked;
  }
  if (!info.updater_state.last_started.is_null()) {
    state->last_started = info.updater_state.last_started;
  }
  state->installation_directory = installation_directory;
  state->policies = info.policies;
  return state;
}

void PopulateUiAppStates(
    std::back_insert_iterator<std::vector<updater_ui::mojom::AppStatePtr>>
        output_iter,
    const std::vector<updater::mojom::AppState>& in_app_states) {
  std::ranges::transform(in_app_states, output_iter,
                         [](const updater::mojom::AppState& app_state) {
                           return updater_ui::mojom::AppState::New(
                               app_state.app_id, app_state.version,
                               app_state.cohort && !app_state.cohort->empty()
                                   ? app_state.cohort
                                   : std::nullopt);
                         });
}

}  // namespace

scoped_refptr<UpdaterPageHandler::Delegate>
UpdaterPageHandler::Delegate::CreateDefault() {
  return base::MakeRefCounted<DefaultUpdaterPageHandlerDelegate>();
}

UpdaterPageHandler::UpdaterPageHandler(
    Profile* profile,
    mojo::PendingReceiver<updater_ui::mojom::PageHandler> receiver,
    mojo::PendingRemote<updater_ui::mojom::Page> page,
    scoped_refptr<Delegate> delegate)
    : profile_(profile),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      delegate_(delegate) {}

UpdaterPageHandler::~UpdaterPageHandler() = default;

void UpdaterPageHandler::GetAllUpdaterEvents(
    GetAllUpdaterEventsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](scoped_refptr<UpdaterPageHandler::Delegate> delegate) {
            std::vector<std::string> all_messages;
            for (const base::FilePath& directory :
                 GetUpdaterDirectories(delegate)) {
              for (const std::string_view filename :
                   {"updater_history.jsonl", "updater_history.jsonl.old"}) {
                std::ranges::move(
                    ReadUpdaterEvents(directory.AppendASCII(filename)),
                    std::back_inserter(all_messages));
              }
            }
            return all_messages;
          },
          delegate_),
      std::move(callback));
}

void UpdaterPageHandler::GetUpdaterStates(GetUpdaterStatesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::optional<base::FilePath> system_install_dir =
      delegate_->GetUpdaterInstallDirectory(updater::UpdaterScope::kSystem);
  std::optional<base::FilePath> user_install_dir =
      delegate_->GetUpdaterInstallDirectory(updater::UpdaterScope::kUser);
  if (!system_install_dir || !user_install_dir) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            base::unexpected(updater_ui::mojom::GetUpdaterStatesError::New())));
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](const base::FilePath& system_install_dir,
             const base::FilePath& user_install_dir) {
            // Attempting to connect to an updater installation which does not
            // exist can cause the proxy to spin until its connection deadline
            // is exhausted. Avoid attempting to connect to updaters which
            // obviously don't exist.
            std::vector<updater::UpdaterScope> present_scopes;
            if (base::PathExists(system_install_dir)) {
              present_scopes.push_back(updater::UpdaterScope::kSystem);
            }
            if (base::PathExists(user_install_dir)) {
              present_scopes.push_back(updater::UpdaterScope::kUser);
            }
            return present_scopes;
          },
          *system_install_dir, *user_install_dir),
      base::BindOnce(
          [](GetUpdaterStatesCallback callback,
             scoped_refptr<UpdaterPageHandler::Delegate> delegate,
             const base::FilePath& system_install_dir,
             const base::FilePath& user_install_dir,
             const std::vector<updater::UpdaterScope>& present_scopes) {
            auto info_by_scope = std::make_unique<
                base::flat_map<updater::UpdaterScope, UpdaterInfo>>();
            auto info_by_scope_ptr = info_by_scope.get();
            base::RepeatingClosure barrier_closure = base::BarrierClosure(
                present_scopes.size(),
                base::BindOnce(
                    [](const base::FilePath& system_install_dir,
                       const base::FilePath& user_install_dir,
                       std::unique_ptr<base::flat_map<
                           updater::UpdaterScope, UpdaterInfo>> info_by_scope) {
                      updater_ui::mojom::GetUpdaterStatesResponsePtr response =
                          updater_ui::mojom::GetUpdaterStatesResponse::New();
                      if (info_by_scope->contains(
                              updater::UpdaterScope::kSystem)) {
                        response->system = ToUpdaterState(
                            info_by_scope->at(updater::UpdaterScope::kSystem),
                            system_install_dir);
                      }
                      if (info_by_scope->contains(
                              updater::UpdaterScope::kUser)) {
                        response->user = ToUpdaterState(
                            info_by_scope->at(updater::UpdaterScope::kUser),
                            user_install_dir);
                      }
                      return response;
                    },
                    system_install_dir, user_install_dir,
                    std::move(info_by_scope))
                    .Then(std::move(callback)));
            for (updater::UpdaterScope scope : present_scopes) {
              GetUpdaterInfo(
                  delegate, scope,
                  base::BindOnce(
                      [](updater::UpdaterScope scope,
                         base::flat_map<updater::UpdaterScope, UpdaterInfo>*
                             out_info_by_scope,
                         const UpdaterInfo& info) {
                        (*out_info_by_scope)[scope] = info;
                      },
                      scope, info_by_scope_ptr)
                      .Then(barrier_closure));
            }
          },
          std::move(callback), delegate_, *system_install_dir,
          *user_install_dir));
}

void UpdaterPageHandler::GetEnterpriseCompanionState(
    GetEnterpriseCompanionStateCallback callback) {
  using updater_ui::mojom::EnterpriseCompanionState;
  using updater_ui::mojom::GetEnterpriseCompanionStateError;
  using updater_ui::mojom::GetEnterpriseCompanionStateResponse;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  delegate_->GetSystemUpdaterAppStates(
      base::BindOnce(
          [](scoped_refptr<Delegate> delegate,
             const std::vector<updater::mojom::AppState>& app_states)
              -> GetEnterpriseCompanionStateResult {
            auto it = std::ranges::find_if(
                app_states, [](const updater::mojom::AppState& app_state) {
                  return base::CompareCaseInsensitiveASCII(
                             app_state.app_id,
                             enterprise_companion::kCompanionAppId) == 0;
                });
            if (it == app_states.end()) {
              return GetEnterpriseCompanionStateResponse::New();
            }

            std::optional<base::FilePath> install_dir =
                delegate->GetEnterpriseCompanionInstallDirectory();
            if (!install_dir) {
              return base::unexpected(GetEnterpriseCompanionStateError::New());
            }

            return GetEnterpriseCompanionStateResponse::New(
                EnterpriseCompanionState::New(
                    /*version=*/it->version, *std::move(install_dir)));
          },
          delegate_)
          .Then(base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void UpdaterPageHandler::GetAppStates(GetAppStatesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  updater_ui::mojom::GetAppStatesResponsePtr response =
      updater_ui::mojom::GetAppStatesResponse::New();
  updater_ui::mojom::GetAppStatesResponse* response_ptr = response.get();
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      2, base::BindOnce(base::BindPostTaskToCurrentDefault(std::move(callback)),
                        std::move(response)));

  delegate_->GetSystemUpdaterAppStates(
      base::BindOnce(&PopulateUiAppStates,
                     std::back_inserter(response_ptr->system_apps))
          .Then(barrier_closure));
  delegate_->GetUserUpdaterAppStates(
      base::BindOnce(&PopulateUiAppStates,
                     std::back_inserter(response_ptr->user_apps))
          .Then(barrier_closure));
}

void UpdaterPageHandler::ShowDirectory(
    updater_ui::mojom::ShowDirectoryTarget target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::optional<base::FilePath> install_dir;
  switch (target) {
    case updater_ui::mojom::ShowDirectoryTarget::kSystemUpdater:
      install_dir =
          delegate_->GetUpdaterInstallDirectory(updater::UpdaterScope::kSystem);
      break;
    case updater_ui::mojom::ShowDirectoryTarget::kUserUpdater:
      install_dir =
          delegate_->GetUpdaterInstallDirectory(updater::UpdaterScope::kUser);
      break;
    case updater_ui::mojom::ShowDirectoryTarget::kEnterpriseCompanionApp:
      install_dir = delegate_->GetEnterpriseCompanionInstallDirectory();
      break;
  }
  if (!install_dir) {
    return;
  }

  platform_util::OpenItem(profile_, *install_dir,
                          platform_util::OpenItemType::OPEN_FOLDER,
                          base::DoNothing());
}
