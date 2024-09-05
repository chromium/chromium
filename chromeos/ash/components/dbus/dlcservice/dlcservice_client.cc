// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"

#include <stdint.h>

#include <algorithm>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/fixed_flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

DlcserviceClient* g_instance = nullptr;

constexpr auto kGetExistingDlcsTimeout = base::Minutes(3);

std::string_view ToDlcServiceError(dbus::ErrorResponse* err_response) {
  const std::string& error_name = err_response->GetErrorName();
  static constexpr auto kErrSet = base::MakeFixedFlatSet<std::string_view>({
      dlcservice::kErrorNone,
      dlcservice::kErrorInternal,
      dlcservice::kErrorBusy,
      dlcservice::kErrorNeedReboot,
      dlcservice::kErrorInvalidDlc,
      dlcservice::kErrorNoImageFound,
  });
  // Lookup the dlcservice error code and provide default on invalid.
  auto itr = kErrSet.find(error_name);
  if (itr == kErrSet.end()) {
    LOG(ERROR) << "Unknown ErrorResponse '" << error_name
               << "', defaulting to kErrorInternal";
    return dlcservice::kErrorInternal;
  }
  return *itr;
}

std::string ToErrorMessage(dbus::ErrorResponse* err_response) {
  std::string err_msg;
  if (!dbus::MessageReader(err_response).PopString(&err_msg)) {
    LOG(ERROR) << "Failed to pop error message from ErrorResponse.";
  }
  return err_msg;
}

std::string_view ParseError(dbus::ErrorResponse* err_response) {
  if (!err_response) {
    LOG(ERROR) << "Failed to parse error, dbus ErrorResponse is null.";
    return dlcservice::kErrorInternal;
  }
  std::string_view err = ToDlcServiceError(err_response);
  std::string err_msg = ToErrorMessage(err_response);
  VLOG(1) << "Handling err=" << err << " err_msg=" << err_msg;
  return err;
}

}  // namespace

// The DlcserviceClient implementation used in production.
class DlcserviceClientImpl : public DlcserviceClient {
 public:
  DlcserviceClientImpl() : dlcservice_proxy_(nullptr) {}

  DlcserviceClientImpl(const DlcserviceClientImpl&) = delete;
  DlcserviceClientImpl& operator=(const DlcserviceClientImpl&) = delete;

  ~DlcserviceClientImpl() override = default;

  void Install(const dlcservice::InstallRequest& install_request,
               InstallCallback install_callback,
               ProgressCallback progress_callback) override {
    const std::string& id = install_request.id();
    VLOG(1) << "DLC install called for: " << id;
    // If another installation for the same DLC ID was already called, go ahead
    // and hold the installation fields.
    if (installation_holder_.find(id) != installation_holder_.end()) {
      LOG(WARNING) << "DLC install is already in progress for: " << id;
      HoldInstallation(install_request, std::move(install_callback),
                       std::move(progress_callback));
      return;
    }
    // TODO(b/220053648): Cleanup all ash-client logic.
    if (installing_) {
      LOG(WARNING) << "First time DLC install is skipping queue for: " << id;
    } else {
      TaskStarted();
    }

    dbus::MethodCall method_call(dlcservice::kDlcServiceInterface,
                                 dlcservice::kInstallMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(install_request);

    VLOG(1) << "Requesting to install DLC(s).";
    // TODO(b/166782419): dlcservice hashes preloadable DLC images which can
    // cause timeouts during preloads. Transitioning into F20 will fix this as
    // preloading will be deprecated.
    constexpr int timeout_ms = 5 * 60 * 1000;
    dlcservice_proxy_->CallMethodWithErrorResponse(
        &method_call, timeout_ms,
        base::BindOnce(&DlcserviceClientImpl::OnInstall,
                       weak_ptr_factory_.GetWeakPtr(), install_request,
                       std::move(install_callback),
                       std::move(progress_callback)));
  }

  void Uninstall(const std::string& dlc_id,
                 UninstallCallback uninstall_callback) override {
    dbus::MethodCall method_call(dlcservice::kDlcServiceInterface,
                                 dlcservice::kUninstallMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(dlc_id);

    VLOG(1) << "Requesting to uninstall DLC=" << dlc_id;
    dlcservice_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DlcserviceClientImpl::OnUninstall,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(uninstall_callback)));
  }

  void Purge(const std::string& dlc_id, PurgeCallback purge_callback) override {
    dbus::MethodCall method_call(dlcservice::kDlcServiceInterface,
                                 dlcservice::kPurgeMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(dlc_id);

    VLOG(1) << "Requesting to purge DLC=" << dlc_id;
    dlcservice_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DlcserviceClientImpl::OnPurge,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(purge_callback)));
  }

  void GetDlcState(const std::string& dlc_id,
                   GetDlcStateCallback callback) override {
    dbus::MethodCall method_call(dlcservice::kDlcServiceInterface,
                                 dlcservice::kGetDlcStateMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(dlc_id);
    VLOG(1) << "Requesting DLC state of" << dlc_id;
    dlcservice_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DlcserviceClientImpl::OnGetDlcState,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetExistingDlcs(GetExistingDlcsCallback callback) override {
    dbus::MethodCall method_call(dlcservice::kDlcServiceInterface,
                                 dlcservice::kGetExistingDlcsMethod);

    VLOG(1) << "Requesting to get existing DLC(s).";
    dlcservice_proxy_->CallMethodWithErrorResponse(
        &method_call, kGetExistingDlcsTimeout.InMilliseconds(),
        base::BindOnce(&DlcserviceClientImpl::OnGetExistingDlcs,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void DlcStateChangedForTest(dbus::Signal* signal) override {
    DlcStateChanged(signal);
  }

  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  void Init(dbus::Bus* bus) {
    dlcservice_proxy_ = bus->GetObjectProxy(
        dlcservice::kDlcServiceServiceName,
        dbus::ObjectPath(dlcservice::kDlcServiceServicePath));
    dlcservice_proxy_->ConnectToSignal(
        dlcservice::kDlcServiceInterface, dlcservice::kDlcStateChangedSignal,
        base::BindRepeating(&DlcserviceClientImpl::DlcStateChanged,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&DlcserviceClientImpl::DlcStateChangedConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    dlcservice_proxy_->WaitForServiceToBeAvailable(
        base::BindOnce(&DlcserviceClientImpl::OnServiceAvailable,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  // Fields related to an installation allowing for multiple installations to be
  // in flight concurrently and handled by this dlcservice client. The callbacks
  // are used to report progress and the final installation.
  struct InstallationHolder {
    InstallCallback install_callback;
    ProgressCallback progress_callback;

    InstallationHolder(InstallCallback install_callback,
                       ProgressCallback progress_callback)
        : install_callback(std::move(install_callback)),
          progress_callback(std::move(progress_callback)) {}
  };

  void OnServiceAvailable(bool service_available) {
    if (service_available) {
      VLOG(1) << "dlcservice is available.";
    } else {
      LOG(ERROR) << "dlcservice is not available.";
    }
    service_available_ = service_available;
  }

  // Set the indication that an install is being performed which was requested
  // from this client (Chrome specifically).
  void TaskStarted() { installing_ = true; }

  // Clears any state an installation had setup while being performed.
  void TaskEnded() { installing_ = false; }

  void HoldInstallation(const dlcservice::InstallRequest& install_request,
                        InstallCallback install_callback,
                        ProgressCallback progress_callback) {
    installation_holder_[install_request.id()].emplace_back(
        std::move(install_callback), std::move(progress_callback));
  }

  void ReleaseInstallation(const std::string& id) {
    installation_holder_.erase(id);
  }

  void EnqueueTask(base::OnceClosure task) {
    pending_tasks_.emplace_back(std::move(task));
  }

  void CheckAndRunPendingTask() {
    // If there are no pending tasks, we can call TaskEnded() now to allow new
    // requests to run immediately.
    if (pending_tasks_.empty()) {
      TaskEnded();
      return;
    }

    // Delay pending tasks and let new tasks get queued to ensure we don't spin
    // the CPU with repeated calls when the DLC installer is busy.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DlcserviceClientImpl::DelayedPendingTask,
                       weak_ptr_factory_.GetWeakPtr()),
        base::Seconds(3));
  }

  void DelayedPendingTask() {
    TaskEnded();
    if (!pending_tasks_.empty()) {
      std::move(pending_tasks_.front()).Run();
      pending_tasks_.pop_front();
    }
  }

  void SendProgress(const dlcservice::DlcState& dlc_state) {
    auto id = dlc_state.id();
    auto progress = dlc_state.progress();
    VLOG(2) << "Installation for DLC " << id << " in progress: " << progress;
    for (auto& installation_state : installation_holder_[id]) {
      installation_state.progress_callback.Run(progress);
    }
  }

  void SendCompleted(const dlcservice::DlcState& dlc_state) {
    auto id = dlc_state.id();
    if (dlc_state.state() == dlcservice::DlcState::NOT_INSTALLED) {
      LOG(ERROR) << "Failed to install DLC " << id
                 << " with error code: " << dlc_state.last_error_code();

    } else {
      VLOG(1) << "DLC " << id << " installed successfully.";
      if (dlc_state.last_error_code() != dlcservice::kErrorNone) {
        LOG(WARNING) << "DLC installation was sucessful but non-success "
                     << "error code: " << dlc_state.last_error_code();
      }
    }

    InstallResult result = {
        .error = dlc_state.last_error_code(),
        .dlc_id = id,
        .root_path = dlc_state.root_path(),
    };
    for (auto& installation_state : installation_holder_[id]) {
      std::move(installation_state.install_callback).Run(result);
    }
    ReleaseInstallation(id);
  }

  void DlcStateChanged(dbus::Signal* signal) {
    dlcservice::DlcState dlc_state;
    if (!dbus::MessageReader(signal).PopArrayOfBytesAsProto(&dlc_state)) {
      LOG(ERROR) << "Failed to parse proto as install status.";
      return;
    }

    // Notify all observers of change in the state of this DLC.
    for (Observer& observer : observers_) {
      observer.OnDlcStateChanged(dlc_state);
    }

    // Skip DLCs not installing from this dlcservice client.
    if (installation_holder_.find(dlc_state.id()) ==
        installation_holder_.end()) {
      return;
    }

    switch (dlc_state.state()) {
      case dlcservice::DlcState::NOT_INSTALLED:
      case dlcservice::DlcState::INSTALLED:
        SendCompleted(dlc_state);
        break;
      case dlcservice::DlcState::INSTALLING:
        SendProgress(dlc_state);
        // Need to return here since we don't want to try starting another
        // pending install from the queue (would waste time checking).
        return;
      default:
        NOTREACHED_IN_MIGRATION();
    }

    // Try to run a pending install since we have complete/failed the current
    // install, but do not waste trying to run a pending install when the
    // current install is running at the moment.
    CheckAndRunPendingTask();
  }

  void DlcStateChangedConnected(const std::string& interface,
                                const std::string& signal,
                                bool success) {
    LOG_IF(ERROR, !success) << "Failed to connect to DlcStateChanged signal.";
  }

  void OnInstall(const dlcservice::InstallRequest& install_request,
                 InstallCallback install_callback,
                 ProgressCallback progress_callback,
                 dbus::Response* response,
                 dbus::ErrorResponse* err_response) {
    const std::string& id = install_request.id();
    if (response) {
      HoldInstallation(install_request, std::move(install_callback),
                       std::move(progress_callback));
      return;
    }

    std::string_view err = ParseError(err_response);
    if (err == dlcservice::kErrorBusy) {
      LOG(WARNING) << "DLC install is getting queued for: " << id;
      EnqueueTask(base::BindOnce(&DlcserviceClientImpl::Install,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 install_request, std::move(install_callback),
                                 std::move(progress_callback)));
    } else {
      HoldInstallation(install_request, std::move(install_callback),
                       std::move(progress_callback));
      dlcservice::DlcState dlc_state;
      dlc_state.set_id(id);
      dlc_state.set_last_error_code(std::string(err));
      SendCompleted(dlc_state);
    }
    CheckAndRunPendingTask();
  }

  void OnUninstall(UninstallCallback uninstall_callback,
                   dbus::Response* response,
                   dbus::ErrorResponse* err_response) {
    std::move(uninstall_callback)
        .Run(response ? dlcservice::kErrorNone : ParseError(err_response));
  }

  void OnPurge(PurgeCallback purge_callback,
               dbus::Response* response,
               dbus::ErrorResponse* err_response) {
    std::move(purge_callback)
        .Run(response ? dlcservice::kErrorNone : ParseError(err_response));
  }

  void OnGetDlcState(GetDlcStateCallback callback,
                     dbus::Response* response,
                     dbus::ErrorResponse* err_response) {
    dlcservice::DlcState dlc_state;
    if (response &&
        dbus::MessageReader(response).PopArrayOfBytesAsProto(&dlc_state)) {
      std::move(callback).Run(dlcservice::kErrorNone, dlc_state);
    } else {
      std::move(callback).Run(ParseError(err_response), dlcservice::DlcState());
    }
  }

  void OnGetExistingDlcs(GetExistingDlcsCallback callback,
                         dbus::Response* response,
                         dbus::ErrorResponse* err_response) {
    dlcservice::DlcsWithContent dlcs_with_content;
    if (response && dbus::MessageReader(response).PopArrayOfBytesAsProto(
                        &dlcs_with_content)) {
      std::move(callback).Run(dlcservice::kErrorNone, dlcs_with_content);
    } else {
      std::move(callback).Run(ParseError(err_response),
                              dlcservice::DlcsWithContent());
    }
  }

  // DLC ID to `InstallationHolder` mapping.
  std::map<std::string, std::vector<InstallationHolder>> installation_holder_;

  raw_ptr<dbus::ObjectProxy> dlcservice_proxy_;

  // TODO(b/220053648): Once platform dlcservice batches, can be removed.
  // Specifically when platform dlcservice doesn't return a busy status.
  // Whether an install is currently in progress. Can be used to decide whether
  // to queue up incoming install requests.
  bool installing_ = false;

  // A list of postponed installs to dlcservice.
  std::deque<base::OnceClosure> pending_tasks_;

  // A list of observers that are listening on state changes, etc.
  base::ObserverList<Observer> observers_;

  // Indicates if dlcservice daemon is available.
  bool service_available_ = false;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DlcserviceClientImpl> weak_ptr_factory_{this};
};

DlcserviceClient::DlcserviceClient() {
  CHECK(!g_instance);
  g_instance = this;
}

DlcserviceClient::~DlcserviceClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void DlcserviceClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new DlcserviceClientImpl())->Init(bus);
}

// static
void DlcserviceClient::InitializeFake() {
  new FakeDlcserviceClient();
}

// static
void DlcserviceClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

// static
DlcserviceClient* DlcserviceClient::Get() {
  return g_instance;
}

}  // namespace ash
