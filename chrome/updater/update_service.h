// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UPDATE_SERVICE_H_
#define CHROME_UPDATER_UPDATE_SERVICE_H_

#include <ostream>
#include <string>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/version.h"
#include "chrome/updater/enum_traits.h"
#include "components/update_client/update_client.h"

namespace updater {

struct RegistrationRequest;
struct RegistrationResponse;

enum class UpdaterScope;

// The UpdateService is the cross-platform core of the updater.
// All functions and callbacks must be called on the same sequence.
class UpdateService : public base::RefCountedThreadSafe<UpdateService> {
 public:
  // Defines the behavior of the update stack for over-installs.
  // Typically, same versions updates are not allowed, in which case, the update
  // server replies with `update not available'. But there are cases, such as
  // re-installing an application again, when the server may respond with an
  // update.
  enum class PolicySameVersionUpdate {
    // The embedder does not allow over-installs with the same version. In this
    // case, the server is expected to return `update not available` when it
    // is queried for updates.
    kNotAllowed = 0,

    // The embedder is capable of handling updates with the same version, and
    // the server may respond with such an update.
    kAllowed = 1,
  };

  // Values posted by the completion |callback| as a result of the
  // non-blocking invocation of the service functions. These values are not
  // present in the telemetry pings.
  enum class Result {
    // Indicates that the service successfully handled the non-blocking function
    // invocation. Returning this value provides no indication regarding the
    // outcome of the function, such as whether the updates succeeded or not.
    kSuccess = 0,

    // The function failed because there is an update in progress. Certain
    // service functions can be parallelized but not all functions can run
    // concurrently.
    kUpdateInProgress = 1,

    // Not used. TODO(crbug.com/1014591).
    kUpdateCanceled = 2,

    // The function failed because of a throttling policy such as load shedding.
    kRetryLater = 3,

    // This is a generic result indicating that an error occurred in the service
    // such as a task failed to post, or allocation of a resource failed.
    kServiceFailed = 4,

    // An error handling the update check occurred.
    kUpdateCheckFailed = 5,

    // This value indicates that required metadata associated with the
    // application was not available for any reason.
    kAppNotFound = 6,

    // A function argument was invalid.
    kInvalidArgument = 7,

    // This server is not the active server.
    kInactive = 8,

    // IPC connection to the remote process failed for some reason.
    kIPCConnectionFailed = 9,

    // Update EnumTraits<UpdateService::Result> when adding new values.
  };

  // Run time errors are organized in specific categories to indicate the
  // component where such errors occurred. The category appears as a numeric
  // value in the telemetry pings. The values of this enum must be kept stable.
  enum class ErrorCategory {
    kNone = 0,
    kDownload = 1,
    kUnpack = 2,
    kInstall = 3,
    kService = 4,
    kUpdateCheck = 5,
    // Update EnumTraits<UpdateService::ErrorCategory> when adding new values.
  };

  struct UpdateState {
    // Possible states for updating an app. Add new values at the end of
    // the definition, and do not mutate the existing values.
    enum class State {
      // This value represents the absence of a state. No update request has
      // yet been issued.
      kUnknown = 0,

      // This update has not been started, but has been requested.
      kNotStarted = 1,

      // The engine began issuing an update check request.
      kCheckingForUpdates = 2,

      // An update is available.
      kUpdateAvailable = 3,

      // The engine began downloading an update.
      kDownloading = 4,

      // The engine began running installation scripts.
      kInstalling = 5,

      // The engine found and installed an update for this product. The update
      // is complete and the state will not change.
      kUpdated = 6,

      // The engine checked for updates. This product is already up to date.
      // No update has been installed for this product. The update is complete
      // and the state will not change.
      kNoUpdate = 7,

      // The engine encountered an error updating this product. The update has
      // halted and the state will not change.
      kUpdateError = 8,

      // Update EnumTraits<UpdateService::UpdateState::State> when adding new
      // values.
    };

    UpdateState();
    UpdateState(const UpdateState&);
    UpdateState& operator=(const UpdateState&);
    UpdateState(UpdateState&&);
    UpdateState& operator=(UpdateState&&);
    ~UpdateState();

    std::string app_id;
    State state = State::kUnknown;

    // The version is initialized only after an update check has completed, and
    // an update is available.
    base::Version next_version;

    int64_t downloaded_bytes = -1;  // -1 means that the byte count is unknown.
    int64_t total_bytes = -1;

    // A value in the range [0, 100] if the install progress is known, or -1
    // if the install progress is not available or it could not be computed.
    int install_progress = -1;

    ErrorCategory error_category = ErrorCategory::kNone;
    int error_code = 0;
    int extra_code1 = 0;
  };

  // Urgency of the update service invocation.
  enum class Priority {
    // The caller has not set a valid priority value.
    kUnknown = 0,

    // The user is not waiting for this update.
    kBackground = 1,

    // The user actively requested this update.
    kForeground = 2,
  };

  struct AppState {
    AppState();
    AppState(const AppState&);
    AppState& operator=(const AppState&);
    AppState(AppState&&);
    AppState& operator=(AppState&&);
    ~AppState();

    std::string app_id;
    base::Version version;
    std::string ap;
    std::string brand_code;
    base::FilePath brand_path;
    base::FilePath ecp;
  };

  using Callback = base::OnceCallback<void(Result)>;
  using StateChangeCallback = base::RepeatingCallback<void(const UpdateState&)>;
  using RegisterAppCallback =
      base::OnceCallback<void(const RegistrationResponse&)>;

  // Returns the version of the active updater. The version object is invalid
  // if an error (including timeout) occurs.
  virtual void GetVersion(base::OnceCallback<void(const base::Version&)>) = 0;

  // Registers given request to the updater.
  virtual void RegisterApp(const RegistrationRequest& request,
                           RegisterAppCallback callback) = 0;

  // Gets state of all registered apps.
  virtual void GetAppStates(
      base::OnceCallback<void(const std::vector<AppState>&)>) = 0;

  // Runs periodic tasks such as checking for uninstallation of registered
  // applications or doing background updates for registered applications.
  virtual void RunPeriodicTasks(base::OnceClosure callback) = 0;

  // Initiates an update check for all registered applications. Receives state
  // change notifications through the repeating |state_update| callback.
  // Calls |callback| once  the operation is complete.
  virtual void UpdateAll(StateChangeCallback state_update,
                         Callback callback) = 0;

  // Updates specified product. This update may be on-demand.
  //
  // Args:
  //   |app_id|: ID of app to update.
  //   |priority|: Priority for processing this update.
  //   |state_update|: The callback will be invoked every time the update
  //     changes state when the engine starts. It will be called on the
  //     sequence used by the update service, so this callback must not block.
  //     It will not be called again after the update has reached a terminal
  //     state. It will not be called after the completion |callback| is posted.
  //   |callback|: Posted after the update stops, successfully or otherwise.
  //
  // |state_update| arg:
  //    UpdateState: the new state of this update request.
  //
  // |callback| arg:
  //    Result: the final result from the update engine.
  virtual void Update(const std::string& app_id,
                      const std::string& install_data_index,
                      Priority priority,
                      PolicySameVersionUpdate policy_same_version_update,
                      StateChangeCallback state_update,
                      Callback callback) = 0;

  // Provides a way to commit data or clean up resources before the task
  // scheduler is shutting down.
  virtual void Uninitialize() = 0;

 protected:
  friend class base::RefCountedThreadSafe<UpdateService>;

  virtual ~UpdateService() = default;
};

// These specializations must be defined in the |updater| namespace.
template <>
struct EnumTraits<UpdateService::Result> {
  using Result = UpdateService::Result;
  static constexpr Result first_elem = Result::kSuccess;
  static constexpr Result last_elem = Result::kIPCConnectionFailed;
};

template <>
struct EnumTraits<UpdateService::UpdateState::State> {
  using State = UpdateService::UpdateState::State;
  static constexpr State first_elem = State::kUnknown;
  static constexpr State last_elem = State::kUpdateError;
};

template <>
struct EnumTraits<UpdateService::ErrorCategory> {
  using ErrorCategory = UpdateService::ErrorCategory;
  static constexpr ErrorCategory first_elem = ErrorCategory::kNone;
  static constexpr ErrorCategory last_elem = ErrorCategory::kUpdateCheck;
};

inline std::ostream& operator<<(std::ostream& os,
                                const UpdateService::Result& result) {
  return os << static_cast<int>(result);
}

std::ostream& operator<<(std::ostream& os,
                         const UpdateService::UpdateState& update_state);

inline std::ostream& operator<<(
    std::ostream& os,
    const UpdateService::PolicySameVersionUpdate& policy_same_version_update) {
  return os << [&policy_same_version_update] {
    switch (policy_same_version_update) {
      case UpdateService::PolicySameVersionUpdate::kNotAllowed:
        return "not allowed";
      case UpdateService::PolicySameVersionUpdate::kAllowed:
        return "allowed";
    }
  }();
}

}  // namespace updater

#endif  // CHROME_UPDATER_UPDATE_SERVICE_H_
