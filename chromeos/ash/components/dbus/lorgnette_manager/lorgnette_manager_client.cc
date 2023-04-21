// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/lorgnette_manager/lorgnette_manager_client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "chromeos/ash/components/dbus/lorgnette/lorgnette_service.pb.h"
#include "chromeos/ash/components/dbus/lorgnette_manager/fake_lorgnette_manager_client.h"
#include "chromeos/dbus/common/pipe_reader.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {
namespace {

LorgnetteManagerClient* g_instance = nullptr;

// The LorgnetteManagerClient implementation used in production.
class LorgnetteManagerClientImpl : public LorgnetteManagerClient {
 public:
  LorgnetteManagerClientImpl() = default;
  LorgnetteManagerClientImpl(const LorgnetteManagerClientImpl&) = delete;
  LorgnetteManagerClientImpl& operator=(const LorgnetteManagerClientImpl&) =
      delete;
  ~LorgnetteManagerClientImpl() override = default;

  void ListScanners(
      chromeos::DBusMethodCallback<lorgnette::ListScannersResponse> callback)
      override {
    dbus::MethodCall method_call(lorgnette::kManagerServiceInterface,
                                 lorgnette::kListScannersMethod);
    lorgnette_daemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&LorgnetteManagerClientImpl::OnListScanners,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetScannerCapabilities(
      const std::string& device_name,
      chromeos::DBusMethodCallback<lorgnette::ScannerCapabilities> callback)
      override {
    dbus::MethodCall method_call(lorgnette::kManagerServiceInterface,
                                 lorgnette::kGetScannerCapabilitiesMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(device_name);
    lorgnette_daemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(
            &LorgnetteManagerClientImpl::OnScannerCapabilitiesResponse,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void StartScan(
      const std::string& device_name,
      const lorgnette::ScanSettings& settings,
      base::OnceCallback<void(lorgnette::ScanFailureMode)> completion_callback,
      base::RepeatingCallback<void(std::string, uint32_t)> page_callback,
      base::RepeatingCallback<void(uint32_t, uint32_t)> progress_callback)
      override {
    lorgnette::StartScanRequest request;
    request.set_device_name(device_name);
    *request.mutable_settings() = settings;

    dbus::MethodCall method_call(lorgnette::kManagerServiceInterface,
                                 lorgnette::kStartScanMethod);
    dbus::MessageWriter writer(&method_call);
    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode StartScanRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(completion_callback),
                                    lorgnette::SCAN_FAILURE_MODE_UNKNOWN));
      return;
    }

    ScanJobState state;
    state.completion_callback = std::move(completion_callback);
    state.progress_callback = std::move(progress_callback);
    state.page_callback = std::move(page_callback);

    lorgnette_daemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&LorgnetteManagerClientImpl::OnStartScanResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(state)));
  }

  void CancelScan(chromeos::VoidDBusMethodCallback cancel_callback) override {
    // Post the task to the proper sequence (since it requires access to
    // scan_job_state_).
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&LorgnetteManagerClientImpl::DoScanCancel,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::move(cancel_callback)));
  }

  void Init(dbus::Bus* bus) override {
    lorgnette_daemon_proxy_ =
        bus->GetObjectProxy(lorgnette::kManagerServiceName,
                            dbus::ObjectPath(lorgnette::kManagerServicePath));
    lorgnette_daemon_proxy_->ConnectToSignal(
        lorgnette::kManagerServiceInterface,
        lorgnette::kScanStatusChangedSignal,
        base::BindRepeating(
            &LorgnetteManagerClientImpl::ScanStatusChangedReceived,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&LorgnetteManagerClientImpl::ScanStatusChangedConnected,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  // Reads scan data on a blocking sequence.
  class ScanDataReader {
   public:
    // In case of success, std::string holds the read data. Otherwise,
    // nullopt.
    using CompletionCallback =
        base::OnceCallback<void(absl::optional<std::string> data)>;

    ScanDataReader() = default;
    ScanDataReader(const ScanDataReader&) = delete;
    ScanDataReader& operator=(const ScanDataReader&) = delete;

    // Creates a pipe to read the scan data from the D-Bus service.
    // Returns a write-side FD.
    base::ScopedFD Start() {
      DCHECK(!pipe_reader_.get());
      DCHECK(!data_.has_value());
      pipe_reader_ = std::make_unique<chromeos::PipeReader>(
          base::ThreadPool::CreateTaskRunner(
              {base::MayBlock(),
               base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}));

      return pipe_reader_->StartIO(base::BindOnce(
          &ScanDataReader::OnDataRead, weak_ptr_factory_.GetWeakPtr()));
    }

    // Waits for the data read completion. If it is already done, |callback|
    // will be called synchronously.
    void Wait(CompletionCallback callback) {
      DCHECK(callback_.is_null());
      callback_ = std::move(callback);
      MaybeCompleted();
    }

   private:
    // Called when a |pipe_reader_| completes reading scan data to a string.
    void OnDataRead(absl::optional<std::string> data) {
      DCHECK(!data_read_);
      data_read_ = true;
      data_ = std::move(data);
      pipe_reader_.reset();
      MaybeCompleted();
    }

    void MaybeCompleted() {
      // If data reading is not yet completed, or D-Bus call does not yet
      // return, wait for the other.
      if (!data_read_ || callback_.is_null())
        return;

      std::move(callback_).Run(std::move(data_));
    }

    std::unique_ptr<chromeos::PipeReader> pipe_reader_;

    // Set to true on data read completion.
    bool data_read_ = false;

    // Available only when |data_read_| is true.
    absl::optional<std::string> data_;

    CompletionCallback callback_;

    base::WeakPtrFactory<ScanDataReader> weak_ptr_factory_{this};
  };

  // The state tracked for an in-progress scan job.
  // Contains callbacks used to report job progress, completion, failure, or
  // cancellation, as well as a ScanDataReader which is responsible for reading
  // from the pipe of data into a string.
  struct ScanJobState {
    base::OnceCallback<void(lorgnette::ScanFailureMode)> completion_callback;
    base::RepeatingCallback<void(uint32_t, uint32_t)> progress_callback;
    base::RepeatingCallback<void(std::string, uint32_t)> page_callback;
    chromeos::VoidDBusMethodCallback cancel_callback;
    std::unique_ptr<ScanDataReader> scan_data_reader;
  };

  // Helper function to send a GetNextImage request to lorgnette for the scan
  // job with the given UUID.
  // Requires that scan_job_state_ contains uuid.
  void GetNextImage(const std::string& uuid) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    lorgnette::GetNextImageRequest request;
    request.set_scan_uuid(uuid);

    ScanJobState& state = scan_job_state_.at(uuid);

    dbus::MethodCall method_call(lorgnette::kManagerServiceInterface,
                                 lorgnette::kGetNextImageMethod);
    dbus::MessageWriter writer(&method_call);
    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode GetNextImageRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(state.completion_callback),
                                    lorgnette::SCAN_FAILURE_MODE_UNKNOWN));
      scan_job_state_.erase(uuid);
      return;
    }

    auto scan_data_reader = std::make_unique<ScanDataReader>();
    base::ScopedFD fd = scan_data_reader->Start();
    writer.AppendFileDescriptor(fd.get());

    state.scan_data_reader = std::move(scan_data_reader);

    lorgnette_daemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&LorgnetteManagerClientImpl::OnGetNextImageResponse,
                       weak_ptr_factory_.GetWeakPtr(), uuid));
  }

  // Helper method to actually perform scan cancellation.
  // We use this method since the scan cancel logic requires that we are running
  // on the proper sequence.
  void DoScanCancel(chromeos::VoidDBusMethodCallback cancel_callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (scan_job_state_.size() == 0) {
      LOG(ERROR) << "No active scan job to cancel.";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(cancel_callback), false));
      return;
    }

    // A more robust implementation would pass a scan job identifier to callers
    // of StartScan() so they could request cancellation of a particular scan.
    if (scan_job_state_.size() > 1) {
      LOG(ERROR) << "Multiple scan jobs running; not clear which to cancel.";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(cancel_callback), false));
      return;
    }

    std::string uuid = scan_job_state_.begin()->first;

    lorgnette::CancelScanRequest request;
    request.set_scan_uuid(uuid);

    dbus::MethodCall method_call(lorgnette::kManagerServiceInterface,
                                 lorgnette::kCancelScanMethod);
    dbus::MessageWriter writer(&method_call);
    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode CancelScanRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(cancel_callback), false));
      return;
    }

    ScanJobState& state = scan_job_state_.begin()->second;
    state.cancel_callback = std::move(cancel_callback);

    lorgnette_daemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&LorgnetteManagerClientImpl::OnCancelScanResponse,
                       weak_ptr_factory_.GetWeakPtr(), uuid));
  }

  // Called when ListScanners completes.
  void OnListScanners(
      chromeos::DBusMethodCallback<lorgnette::ListScannersResponse> callback,
      dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to obtain ListScannersResponse";
      std::move(callback).Run(absl::nullopt);
      return;
    }

    lorgnette::ListScannersResponse response_proto;
    dbus::MessageReader reader(response);
    if (!reader.PopArrayOfBytesAsProto(&response_proto)) {
      LOG(ERROR) << "Failed to read ListScannersResponse";
      std::move(callback).Run(absl::nullopt);
      return;
    }

    std::move(callback).Run(std::move(response_proto));
  }

  // Handles the response received after calling GetScannerCapabilities().
  void OnScannerCapabilitiesResponse(
      chromeos::DBusMethodCallback<lorgnette::ScannerCapabilities> callback,
      dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to obtain ScannerCapabilities";
      std::move(callback).Run(absl::nullopt);
      return;
    }

    lorgnette::ScannerCapabilities response_proto;
    dbus::MessageReader reader(response);
    if (!reader.PopArrayOfBytesAsProto(&response_proto)) {
      LOG(ERROR) << "Failed to read ScannerCapabilities";
      std::move(callback).Run(absl::nullopt);
      return;
    }

    std::move(callback).Run(std::move(response_proto));
  }

  // Called when scan data read is completed.
  void OnScanDataCompleted(const std::string& uuid,
                           uint32_t page_number,
                           bool more_pages,
                           absl::optional<std::string> data) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!base::Contains(scan_job_state_, uuid)) {
      LOG(ERROR) << "Received ScanDataCompleted for unrecognized scan job: "
                 << uuid;
      return;
    }

    ScanJobState& state = scan_job_state_[uuid];
    if (!data.has_value()) {
      LOG(ERROR) << "Reading scan data failed";
      std::move(state.completion_callback)
          .Run(lorgnette::SCAN_FAILURE_MODE_UNKNOWN);
      scan_job_state_.erase(uuid);
      return;
    }

    state.page_callback.Run(std::move(data.value()), page_number);

    if (more_pages) {
      GetNextImage(uuid);
    } else {
      std::move(state.completion_callback)
          .Run(lorgnette::SCAN_FAILURE_MODE_NO_FAILURE);
      scan_job_state_.erase(uuid);
    }
  }

  void OnStartScanResponse(ScanJobState state, dbus::Response* response) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!response) {
      LOG(ERROR) << "Failed to obtain StartScanResponse";
      std::move(state.completion_callback)
          .Run(lorgnette::SCAN_FAILURE_MODE_UNKNOWN);
      return;
    }

    lorgnette::StartScanResponse response_proto;
    dbus::MessageReader reader(response);
    if (!reader.PopArrayOfBytesAsProto(&response_proto)) {
      LOG(ERROR) << "Failed to decode StartScanResponse proto";
      std::move(state.completion_callback)
          .Run(lorgnette::SCAN_FAILURE_MODE_UNKNOWN);
      return;
    }

    if (response_proto.state() == lorgnette::SCAN_STATE_FAILED) {
      LOG(ERROR) << "Starting Scan failed: " << response_proto.failure_reason();
      std::move(state.completion_callback)
          .Run(response_proto.scan_failure_mode());
      return;
    }

    scan_job_state_[response_proto.scan_uuid()] = std::move(state);
    GetNextImage(response_proto.scan_uuid());
  }

  void OnCancelScanResponse(const std::string& scan_uuid,
                            dbus::Response* response) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // If the cancel completed and the scan job has been erased, there's no work
    // to do.
    auto it = scan_job_state_.find(scan_uuid);
    if (it == scan_job_state_.end())
      return;

    ScanJobState& state = it->second;
    if (state.cancel_callback.is_null()) {
      LOG(ERROR) << "No callback active to cancel job " << scan_uuid;
      return;
    }
    if (!response) {
      LOG(ERROR) << "Failed to obtain CancelScanResponse";
      std::move(state.cancel_callback).Run(false);
      return;
    }

    lorgnette::CancelScanResponse response_proto;
    dbus::MessageReader reader(response);
    if (!reader.PopArrayOfBytesAsProto(&response_proto)) {
      LOG(ERROR) << "Failed to decode CancelScanResponse proto";
      std::move(state.cancel_callback).Run(false);
      return;
    }

    // If the cancel request failed, report the cancel as failed via the
    // callback. Otherwise, wait for the cancel to complete.
    if (!response_proto.success()) {
      LOG(ERROR) << "Cancelling scan failed: "
                 << response_proto.failure_reason();
      std::move(state.cancel_callback).Run(false);
      return;
    }
  }

  // Called when a response to a GetNextImage request is received from
  // lorgnette. Handles stopping the scan if the request failed.
  void OnGetNextImageResponse(std::string uuid, dbus::Response* response) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // If the scan was canceled and the scan job has been erased, there's no
    // need to check the next image response.
    auto it = scan_job_state_.find(uuid);
    if (it == scan_job_state_.end())
      return;

    ScanJobState& state = it->second;
    if (!response) {
      LOG(ERROR) << "Failed to obtain GetNextImage response";
      std::move(state.completion_callback)
          .Run(lorgnette::SCAN_FAILURE_MODE_UNKNOWN);
      scan_job_state_.erase(uuid);
      return;
    }

    lorgnette::GetNextImageResponse response_proto;
    dbus::MessageReader reader(response);
    if (!reader.PopArrayOfBytesAsProto(&response_proto)) {
      LOG(ERROR) << "Failed to decode GetNextImageResponse proto";
      std::move(state.completion_callback)
          .Run(lorgnette::SCAN_FAILURE_MODE_UNKNOWN);
      scan_job_state_.erase(uuid);
      return;
    }

    if (!response_proto.success()) {
      LOG(ERROR) << "Getting next image failed: "
                 << response_proto.failure_reason();
      std::move(state.completion_callback)
          .Run(response_proto.scan_failure_mode());
      scan_job_state_.erase(uuid);
      return;
    }
  }

  void ScanStatusChangedReceived(dbus::Signal* signal) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    dbus::MessageReader reader(signal);
    lorgnette::ScanStatusChangedSignal signal_proto;
    if (!reader.PopArrayOfBytesAsProto(&signal_proto)) {
      LOG(ERROR) << "Failed to decode ScanStatusChangedSignal proto";
      return;
    }

    if (!base::Contains(scan_job_state_, signal_proto.scan_uuid())) {
      LOG(ERROR) << "Received signal for unrecognized scan job: "
                 << signal_proto.scan_uuid();
      return;
    }
    ScanJobState& state = scan_job_state_[signal_proto.scan_uuid()];

    if (signal_proto.state() == lorgnette::SCAN_STATE_FAILED) {
      LOG(ERROR) << "Scan job " << signal_proto.scan_uuid()
                 << " failed: " << signal_proto.failure_reason();
      std::move(state.completion_callback)
          .Run(signal_proto.scan_failure_mode());
      scan_job_state_.erase(signal_proto.scan_uuid());
    } else if (signal_proto.state() == lorgnette::SCAN_STATE_PAGE_COMPLETED) {
      VLOG(1) << "Scan job " << signal_proto.scan_uuid() << " page "
              << signal_proto.page() << " completed successfully";
      ScanDataReader* scan_data_reader = state.scan_data_reader.get();
      scan_data_reader->Wait(base::BindOnce(
          &LorgnetteManagerClientImpl::OnScanDataCompleted,
          weak_ptr_factory_.GetWeakPtr(), signal_proto.scan_uuid(),
          signal_proto.page(), signal_proto.more_pages()));
    } else if (signal_proto.state() == lorgnette::SCAN_STATE_COMPLETED) {
      VLOG(1) << "Scan job " << signal_proto.scan_uuid()
              << " completed successfully";
    } else if (signal_proto.state() == lorgnette::SCAN_STATE_IN_PROGRESS &&
               !state.progress_callback.is_null()) {
      state.progress_callback.Run(signal_proto.progress(), signal_proto.page());
    } else if (signal_proto.state() == lorgnette::SCAN_STATE_CANCELLED) {
      VLOG(1) << "Scan job " << signal_proto.scan_uuid()
              << " has been cancelled.";
      if (!state.cancel_callback.is_null())
        std::move(state.cancel_callback).Run(true);

      scan_job_state_.erase(signal_proto.scan_uuid());
    }
  }

  void ScanStatusChangedConnected(const std::string& interface_name,
                                  const std::string& signal_name,
                                  bool success) {
    LOG_IF(WARNING, !success)
        << "Failed to connect to ScanStatusChanged signal.";
  }

  raw_ptr<dbus::ObjectProxy, ExperimentalAsh> lorgnette_daemon_proxy_ = nullptr;

  // Map from scan UUIDs to ScanDataReader and callbacks for reporting scan
  // progress and completion.
  base::flat_map<std::string, ScanJobState> scan_job_state_
      GUARDED_BY_CONTEXT(sequence_checker_);
  // Ensures that all callbacks are handled on the same sequence, so that it is
  // safe to access scan_job_state_ without a lock.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<LorgnetteManagerClientImpl> weak_ptr_factory_{this};
};

}  // namespace

// static
void LorgnetteManagerClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new LorgnetteManagerClientImpl())->Init(bus);
}

// static
void LorgnetteManagerClient::InitializeFake() {
  new FakeLorgnetteManagerClient();
}

// static
void LorgnetteManagerClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

// static
LorgnetteManagerClient* LorgnetteManagerClient::Get() {
  return g_instance;
}

LorgnetteManagerClient::LorgnetteManagerClient() {
  CHECK(!g_instance);
  g_instance = this;
}

LorgnetteManagerClient::~LorgnetteManagerClient() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace ash
