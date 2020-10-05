// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/lorgnette_manager_client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/files/scoped_file.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromeos/dbus/lorgnette/lorgnette_service.pb.h"
#include "chromeos/dbus/pipe_reader.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// The LorgnetteManagerClient implementation used in production.
class LorgnetteManagerClientImpl : public LorgnetteManagerClient {
 public:
  LorgnetteManagerClientImpl() = default;
  LorgnetteManagerClientImpl(const LorgnetteManagerClientImpl&) = delete;
  LorgnetteManagerClientImpl& operator=(const LorgnetteManagerClientImpl&) =
      delete;
  ~LorgnetteManagerClientImpl() override = default;

  void ListScanners(
      DBusMethodCallback<lorgnette::ListScannersResponse> callback) override {
    dbus::MethodCall method_call(lorgnette::kManagerServiceInterface,
                                 lorgnette::kListScannersMethod);
    lorgnette_daemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&LorgnetteManagerClientImpl::OnListScanners,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetScannerCapabilities(
      const std::string& device_name,
      DBusMethodCallback<lorgnette::ScannerCapabilities> callback) override {
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

  // LorgnetteManagerClient override.
  void StartScan(
      const std::string& device_name,
      const lorgnette::ScanSettings& settings,
      VoidDBusMethodCallback completion_callback,
      base::RepeatingCallback<void(std::string, uint32_t)> page_callback,
      base::RepeatingCallback<void(int)> progress_callback) override {
    lorgnette::StartScanRequest request;
    request.set_device_name(device_name);
    *request.mutable_settings() = settings;

    dbus::MethodCall method_call(lorgnette::kManagerServiceInterface,
                                 lorgnette::kStartScanMethod);
    dbus::MessageWriter writer(&method_call);
    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode StartScanRequest protobuf";
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(completion_callback), false));
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

 protected:
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
        base::OnceCallback<void(base::Optional<std::string> data)>;

    ScanDataReader() = default;

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
    void OnDataRead(base::Optional<std::string> data) {
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
    base::Optional<std::string> data_;

    CompletionCallback callback_;

    base::WeakPtrFactory<ScanDataReader> weak_ptr_factory_{this};
    DISALLOW_COPY_AND_ASSIGN(ScanDataReader);
  };

  // The state tracked for an in-progress scan job.
  // Contains callbacks used to report progress and job completion or failure,
  // as well as a ScanDataReader which is responsible for reading from the pipe
  // of data into a string.
  struct ScanJobState {
    VoidDBusMethodCallback completion_callback;
    base::RepeatingCallback<void(int)> progress_callback;
    base::RepeatingCallback<void(std::string, uint32_t)> page_callback;
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
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(state.completion_callback), false));
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

  // Called when ListScanners completes.
  void OnListScanners(
      DBusMethodCallback<lorgnette::ListScannersResponse> callback,
      dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to obtain ListScannersResponse";
      std::move(callback).Run(base::nullopt);
      return;
    }

    lorgnette::ListScannersResponse response_proto;
    dbus::MessageReader reader(response);
    if (!reader.PopArrayOfBytesAsProto(&response_proto)) {
      LOG(ERROR) << "Failed to read ListScannersResponse";
      std::move(callback).Run(base::nullopt);
      return;
    }

    std::move(callback).Run(std::move(response_proto));
  }

  // Handles the response received after calling GetScannerCapabilities().
  void OnScannerCapabilitiesResponse(
      DBusMethodCallback<lorgnette::ScannerCapabilities> callback,
      dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to obtain ScannerCapabilities";
      std::move(callback).Run(base::nullopt);
      return;
    }

    lorgnette::ScannerCapabilities response_proto;
    dbus::MessageReader reader(response);
    if (!reader.PopArrayOfBytesAsProto(&response_proto)) {
      LOG(ERROR) << "Failed to read ScannerCapabilities";
      std::move(callback).Run(base::nullopt);
      return;
    }

    std::move(callback).Run(std::move(response_proto));
  }

  // Called when scan data read is completed.
  void OnScanDataCompleted(const std::string& uuid,
                           uint32_t page_number,
                           bool more_pages,
                           base::Optional<std::string> data) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!base::Contains(scan_job_state_, uuid)) {
      LOG(ERROR) << "Received ScanDataCompleted for unrecognized scan job: "
                 << uuid;
      return;
    }

    ScanJobState& state = scan_job_state_[uuid];
    if (!data.has_value()) {
      LOG(ERROR) << "Reading scan data failed";
      std::move(state.completion_callback).Run(false);
      scan_job_state_.erase(uuid);
      return;
    }

    state.page_callback.Run(std::move(data.value()), page_number);

    if (more_pages) {
      GetNextImage(uuid);
    } else {
      std::move(state.completion_callback).Run(true);
      scan_job_state_.erase(uuid);
    }
  }

  void OnStartScanResponse(ScanJobState state, dbus::Response* response) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!response) {
      LOG(ERROR) << "Failed to obtain StartScanResponse";
      std::move(state.completion_callback).Run(false);
      return;
    }

    lorgnette::StartScanResponse response_proto;
    dbus::MessageReader reader(response);
    if (!reader.PopArrayOfBytesAsProto(&response_proto)) {
      LOG(ERROR) << "Failed to decode StartScanResponse proto";
      std::move(state.completion_callback).Run(false);
      return;
    }

    if (response_proto.state() == lorgnette::SCAN_STATE_FAILED) {
      LOG(ERROR) << "Starting Scan failed: " << response_proto.failure_reason();
      std::move(state.completion_callback).Run(false);
      return;
    }

    scan_job_state_[response_proto.scan_uuid()] = std::move(state);
    GetNextImage(response_proto.scan_uuid());
  }

  // Callend when a response to a GetNextImage request is received from
  // lorgnette. Handles stopping the scan if the request failed.
  void OnGetNextImageResponse(std::string uuid, dbus::Response* response) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    ScanJobState& state = scan_job_state_[uuid];
    if (!response) {
      LOG(ERROR) << "Failed to obtain GetNextImage response";
      std::move(state.completion_callback).Run(false);
      scan_job_state_.erase(uuid);
      return;
    }

    lorgnette::StartScanResponse response_proto;
    dbus::MessageReader reader(response);
    if (!reader.PopArrayOfBytesAsProto(&response_proto)) {
      LOG(ERROR) << "Failed to decode GetNextImageResponse proto";
      std::move(state.completion_callback).Run(false);
      scan_job_state_.erase(uuid);
      return;
    }

    if (response_proto.state() == lorgnette::SCAN_STATE_FAILED) {
      LOG(ERROR) << "Getting next image failed: "
                 << response_proto.failure_reason();
      std::move(state.completion_callback).Run(false);
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
      std::move(state.completion_callback).Run(false);
      scan_job_state_.erase(signal_proto.scan_uuid());
    } else if (signal_proto.state() == lorgnette::SCAN_STATE_PAGE_COMPLETED) {
      VLOG(1) << "Scan job " << signal_proto.scan_uuid() << " page "
              << signal_proto.page() << " completed successfully";
      ScanDataReader* reader = state.scan_data_reader.get();
      reader->Wait(base::BindOnce(
          &LorgnetteManagerClientImpl::OnScanDataCompleted,
          weak_ptr_factory_.GetWeakPtr(), signal_proto.scan_uuid(),
          signal_proto.page(), signal_proto.more_pages()));
    } else if (signal_proto.state() == lorgnette::SCAN_STATE_COMPLETED) {
      VLOG(1) << "Scan job " << signal_proto.scan_uuid()
              << " completed successfully";
    } else if (signal_proto.state() == lorgnette::SCAN_STATE_IN_PROGRESS &&
               !state.progress_callback.is_null()) {
      state.progress_callback.Run(signal_proto.progress());
    }
  }

  void ScanStatusChangedConnected(const std::string& interface_name,
                                  const std::string& signal_name,
                                  bool success) {
    LOG_IF(WARNING, !success)
        << "Failed to connect to ScanStatusChanged signal.";
  }

  dbus::ObjectProxy* lorgnette_daemon_proxy_ = nullptr;

  // Map from scan UUIDs to ScanDataReader and callbacks for reporting scan
  // progress and completion.
  base::flat_map<std::string, ScanJobState> scan_job_state_
      GUARDED_BY_CONTEXT(sequence_checker_);
  // Ensures that all callbacks are handled on the same sequence, so that it is
  // safe to access scan_job_state_ without a lock.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<LorgnetteManagerClientImpl> weak_ptr_factory_{this};
};

LorgnetteManagerClient::LorgnetteManagerClient() = default;

LorgnetteManagerClient::~LorgnetteManagerClient() = default;

// static
std::unique_ptr<LorgnetteManagerClient> LorgnetteManagerClient::Create() {
  return std::make_unique<LorgnetteManagerClientImpl>();
}

}  // namespace chromeos
