// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UPDATE_SERVICE_H_
#define CHROME_UPDATER_UPDATE_SERVICE_H_

#include <ostream>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/version.h"
#include "chrome/updater/enum_traits.h"
#include "chrome/updater/mojom/updater_service.mojom.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/util/util.h"
#include "components/update_client/update_client.h"

namespace policy {
enum class PolicyFetchReason;
}  // namespace policy

namespace updater {

using RpcError = int;

enum class UpdaterScope;

// The UpdateService is the cross-platform core of the updater.
// All functions and callbacks must be called on the same sequence.
class UpdateService : public base::RefCountedThreadSafe<UpdateService> {
 public:
  using PolicySameVersionUpdate = mojom::UpdateService::PolicySameVersionUpdate;
  using Result = mojom::UpdateService::Result;
  using ErrorCategory = mojom::UpdateService::ErrorCategory;
  using UpdateState = mojom::UpdateState;
  using Priority = mojom::UpdateService::Priority;
  using AppState = mojom::AppState;

  // Returns the version of the active updater. The version object is invalid
  // if an error (including timeout) occurs.
  virtual void GetVersion(base::OnceCallback<void(const base::Version&)>) = 0;

  // Fetches policies from device management.
  virtual void FetchPolicies(policy::PolicyFetchReason reason,
                             base::OnceCallback<void(int)> callback) = 0;

  // Registers given request to the updater.
  virtual void RegisterApp(const RegistrationRequest& request,
                           base::OnceCallback<void(int)> callback) = 0;

  // Gets state of all registered apps.
  virtual void GetAppStates(
      base::OnceCallback<void(const std::vector<AppState>&)>) = 0;

  // Runs periodic tasks such as checking for uninstallation of registered
  // applications or doing background updates for registered applications.
  virtual void RunPeriodicTasks(base::OnceClosure callback) = 0;

  // DEPRECATED - queries the server for updates, gets an update response, but
  // does not download, install, or run any payload. This is intended to
  // support the legacy on-demand Omaha interface but it should not be used
  // by new code. The parameters are similar to the parameters of `Update`.
  virtual void CheckForUpdate(
      const std::string& app_id,
      Priority priority,
      PolicySameVersionUpdate policy_same_version_update,
      const std::string& language,
      base::RepeatingCallback<void(const UpdateState&)> state_update,
      base::OnceCallback<void(Result)> callback) = 0;

  // Updates specified product. This update may be on-demand.
  //
  // Args:
  //   `app_id`: ID of app to update.
  //   `install_data_index`: Index of the server install data.
  //   `priority`: Priority for processing this update.
  //   `policy_same_version_update`: Whether a same-version update is allowed.
  //   `language`: The UI language for the update.
  //   `state_update`: The callback will be invoked every time the update
  //     changes state when the engine starts. It will be called on the
  //     sequence used by the update service, so this callback must not block.
  //     It will not be called again after the update has reached a terminal
  //     state. It will not be called after the completion `callback` is posted.
  //   `callback`: Posted after the update stops, successfully or otherwise.
  //
  //   `state_update` arg:
  //     UpdateState: the new state of this update request.
  //
  //   `callback` arg:
  //     Result: the final result from the update engine.
  virtual void Update(
      const std::string& app_id,
      const std::string& install_data_index,
      Priority priority,
      PolicySameVersionUpdate policy_same_version_update,
      const std::string& language,
      base::RepeatingCallback<void(const UpdateState&)> state_update,
      base::OnceCallback<void(Result)> callback) = 0;

  // Initiates an update check for all registered applications. Receives state
  // change notifications through the repeating `state_update` callback.
  // Calls `callback` once  the operation is complete.
  virtual void UpdateAll(
      base::RepeatingCallback<void(const UpdateState&)> state_update,
      base::OnceCallback<void(Result)> callback) = 0;

  // Registers and installs an application from the network.
  //
  // Args:
  //   `registration`: Registration data about the app.
  //   `client_install_data`: User provided install data.
  //   `install_data_index`: Index of the server install data. Effective only
  //     when `client_install_data` is not set.
  //   `priority`: Priority for processing this update.
  //   `language`: The UI language for the install.
  //   `state_update`: The callback will be invoked every time the update
  //     changes state when the engine starts. It will be called on the
  //     sequence used by the update service, so this callback must not block.
  //     It will not be called again after the update has reached a terminal
  //     state. It will not be called after the completion `callback` is posted.
  //   `callback`: Posted after the update stops, successfully or otherwise.
  //
  //   `state_update` arg:
  //     UpdateState: the new state of this update request.
  //
  //   `callback` arg:
  //     Result: the final result from the update engine.
  virtual void Install(
      const RegistrationRequest& registration,
      const std::string& client_install_data,
      const std::string& install_data_index,
      Priority priority,
      const std::string& language,
      base::RepeatingCallback<void(const UpdateState&)> state_update,
      base::OnceCallback<void(Result)> callback) = 0;

  // Cancels any ongoing installations of the specified product. This does not
  // interrupt any product installers that are currently running, but does
  // prevent them from being run if they are not yet downloaded.
  //
  // Args:
  //   `app_id`: ID of the product to cancel installs of.
  virtual void CancelInstalls(const std::string& app_id) = 0;

  // Install an app by running its installer.
  //
  // Args:
  //   `app_id`: ID of app to install.
  //   `app_installer`: Offline installer path.
  //   `arguments`: Arguments to run the installer.
  //   `install_data`: Server install data extracted from the offline manifest.
  //   `install_settings`: An optional serialized dictionary to customize the
  //       installation.
  //   `language`: The UI language for the install.
  //   `state_update` arg:
  //     UpdateState: the new state of this install request.
  //
  //   `callback` arg:
  //     Result: the final result from the update engine.
  virtual void RunInstaller(
      const std::string& app_id,
      const base::FilePath& installer_path,
      const std::string& install_args,
      const std::string& install_data,
      const std::string& install_settings,
      const std::string& language,
      base::RepeatingCallback<void(const UpdateState&)> state_update,
      base::OnceCallback<void(Result)> callback) = 0;

 protected:
  friend class base::RefCountedThreadSafe<UpdateService>;

  virtual ~UpdateService() = default;
};

// These specializations must be defined in the `updater` namespace.
template <>
struct EnumTraits<UpdateService::Result> {
  using Result = UpdateService::Result;
  static constexpr Result first_elem = Result::kSuccess;
  static constexpr Result last_elem = Result::kFetchPoliciesFailed;
};

template <>
struct EnumTraits<UpdateService::UpdateState::State> {
  using State = UpdateService::UpdateState::State;
  static constexpr State first_elem = State::kUnknown;
  static constexpr State last_elem = State::kPatching;
};

template <>
struct EnumTraits<UpdateService::ErrorCategory> {
  using ErrorCategory = UpdateService::ErrorCategory;
  static constexpr ErrorCategory first_elem = ErrorCategory::kNone;
  static constexpr ErrorCategory last_elem = ErrorCategory::kInstaller;
};

std::ostream& operator<<(std::ostream& os,
                         const UpdateService::UpdateState& update_state);

bool operator==(const UpdateService::UpdateState& lhs,
                const UpdateService::UpdateState& rhs);

}  // namespace updater

#endif  // CHROME_UPDATER_UPDATE_SERVICE_H_
