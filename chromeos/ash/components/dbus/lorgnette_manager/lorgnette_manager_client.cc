// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/lorgnette_manager/lorgnette_manager_client.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/dbus/lorgnette/lorgnette_service.pb.h"
#include "chromeos/ash/components/dbus/lorgnette_manager/fake_lorgnette_manager_client.h"
#include "chromeos/dbus/common/pipe_reader.h"
#include "components/device_event_log/device_event_log.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {
namespace {

LorgnetteManagerClient* g_instance = nullptr;

constexpr base::TimeDelta kDiscoveryMonitorInterval = base::Seconds(1);
constexpr base::TimeDelta kDiscoveryMaxInactivity = base::Seconds(20);
constexpr base::TimeDelta kSlowOperationTimeout = base::Seconds(60);

// The LorgnetteManagerClient implementation used in production.
class LorgnetteManagerClientImpl : public LorgnetteManagerClient {
 public:
  LorgnetteManagerClientImpl() = default;
  LorgnetteManagerClientImpl(const LorgnetteManagerClientImpl&) = delete;
  LorgnetteManagerClientImpl& operator=(const LorgnetteManagerClientImpl&) =
      delete;
  ~LorgnetteManagerClientImpl() override = default;

  void ListScanners(
      const std::string& client_id,
      bool local_only,
      bool preferred_only,
      chromeos::DBusMethodCallback<lorgnette::ListScannersResponse> callback)
      override {
    // The client ID is required for asynchronous discovery.  If none is
    // provided, exit early with an error result.
    if (client_id.empty()) {
      lorgnette::ListScannersResponse response;
      response.set_result(lorgnette::OPERATION_RESULT_INVALID);
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::move(response)));
      return;
    }

    lorgnette::StartScannerDiscoveryRequest request;
    request.set_client_id(client_id);
    request.set_preferred_only(preferred_only);
    request.set_local_only(local_only);

    StartScannerDiscovery(
        std::move(request),
        base::BindRepeating(
            &LorgnetteManagerClientImpl::ListScannersDiscoveryScannersUpdated,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(
            &LorgnetteManagerClientImpl::OnListScannersDiscoverySession,
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

  void OpenScanner(const lorgnette::OpenScannerRequest& request,
                   chromeos::DBusMethodCallback<lorgnette::OpenScannerResponse>
                       callback) override {
    dbus::MethodCall method_call(lorgnette::kManagerServiceInterface,
                                 lorgnette::kOpenScannerMethod);
    dbus::MessageWriter writer(&method_call);
    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode OpenScannerRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }
    lorgnette_daemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&LorgnetteManagerClientImpl::OnOpenScannerResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void CloseScanner(
      const lorgnette::CloseScannerRequest& request,
      chromeos::DBusMethodCallback<lorgnette::CloseScannerResponse> callback)
      override {
    dbus::MethodCall method_call(lorgnette::kManagerServiceInterface,
                                 lorgnette::kCloseScannerMethod);
    dbus::MessageWriter writer(&method_call);
    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode CloseScannerRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }
    lorgnette_daemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&LorgnetteManagerClientImpl::OnCloseScannerResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void SetOptions(const lorgnette::SetOptionsRequest& request,
                  chromeos::DBusMethodCallback<lorgnette::SetOptionsResponse>
                      callback) override {
    dbus::MethodCall method_call(lorgnette::kManagerServiceInterface,
                                 lorgnette::kSetOptionsMethod);
    dbus::MessageWriter writer(&method_call);
    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode SetOptionsRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }
    lorgnette_daemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&LorgnetteManagerClientImpl::OnSetOptionsResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetCurrentConfig(
      const lorgnette::GetCurrentConfigRequest& request,
      chromeos::DBusMethodCallback<lorgnette::GetCurrentConfigResponse>
          callback) override {
    dbus::MethodCall method_call(lorgnette::kManagerServiceInterface,
                                 lorgnette::kGetCurrentConfigMethod);
    dbus::MessageWriter writer(&method_call);
    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode GetCurrentConfigRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }
    lorgnette_daemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&LorgnetteManagerClientImpl::OnGetCurrentConfigResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void StartPreparedScan(
      const lorgnette::StartPreparedScanRequest& request,
      chromeos::DBusMethodCallback<lorgnette::StartPreparedScanResponse>
          callback) override {
    dbus::MethodCall method_call(lorgnette::kManagerServiceInterface,
                                 lorgnette::kStartPreparedScanMethod);
    dbus::MessageWriter writer(&method_call);
    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode StartPreparedScanRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }
    lorgnette_daemon_proxy_->CallMethod(
        &method_call, kSlowOperationTimeout.InMilliseconds(),
        base::BindOnce(&LorgnetteManagerClientImpl::OnStartPreparedScanResponse,
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

  void ReadScanData(
      const lorgnette::ReadScanDataRequest& request,
      chromeos::DBusMethodCallback<lorgnette::ReadScanDataResponse> callback)
      override {
    dbus::MethodCall method_call(lorgnette::kManagerServiceInterface,
                                 lorgnette::kReadScanDataMethod);
    dbus::MessageWriter writer(&method_call);
    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode ReadScanDataRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }
    lorgnette_daemon_proxy_->CallMethod(
        &method_call, kSlowOperationTimeout.InMilliseconds(),
        base::BindOnce(&LorgnetteManagerClientImpl::OnReadScanDataResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void CancelScan(chromeos::VoidDBusMethodCallback cancel_callback) override {
    // Post the task to the proper sequence (since it requires access to
    // scan_job_state_).
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&LorgnetteManagerClientImpl::DoScanCancel,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::move(cancel_callback)));
  }

  void CancelScan(const lorgnette::CancelScanRequest& request,
                  chromeos::DBusMethodCallback<lorgnette::CancelScanResponse>
                      callback) override {
    dbus::MethodCall method_call(lorgnette::kManagerServiceInterface,
                                 lorgnette::kCancelScanMethod);
    dbus::MessageWriter writer(&method_call);
    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode CancelScanRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }
    lorgnette_daemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&LorgnetteManagerClientImpl::OnCancelScanJobResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void StartScannerDiscovery(
      const lorgnette::StartScannerDiscoveryRequest& request,
      base::RepeatingCallback<void(lorgnette::ScannerListChangedSignal)>
          signal_callback,
      chromeos::DBusMethodCallback<lorgnette::StartScannerDiscoveryResponse>
          response_callback) override {
    dbus::MethodCall method_call(lorgnette::kManagerServiceInterface,
                                 lorgnette::kStartScannerDiscoveryMethod);
    dbus::MessageWriter writer(&method_call);
    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode StartScannerDiscoveryRequest protobuf";

      lorgnette::StartScannerDiscoveryResponse response;
      response.set_started(false);

      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(response_callback), std::move(response)));
      return;
    }

    PRINTER_LOG(USER) << "Starting scanner discovery for client "
                      << request.client_id()
                      << ", local_only=" << request.local_only()
                      << ", preferred_only=" << request.preferred_only();

    lorgnette_daemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(
            &LorgnetteManagerClientImpl::OnStartScannerDiscoveryResponse,
            weak_ptr_factory_.GetWeakPtr(), std::move(request),
            std::move(signal_callback), std::move(response_callback)));
  }

  void StopScannerDiscovery(
      const lorgnette::StopScannerDiscoveryRequest& request,
      chromeos::DBusMethodCallback<lorgnette::StopScannerDiscoveryResponse>
          callback) override {
    dbus::MethodCall method_call(lorgnette::kManagerServiceInterface,
                                 lorgnette::kStopScannerDiscoveryMethod);
    dbus::MessageWriter writer(&method_call);
    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode StopScannerDiscoveryRequest protobuf";

      lorgnette::StopScannerDiscoveryResponse response;
      response.set_stopped(false);

      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::move(response)));
      return;
    }

    lorgnette_daemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(
            &LorgnetteManagerClientImpl::OnStopScannerDiscoveryResponse,
            weak_ptr_factory_.GetWeakPtr(), request.session_id(),
            std::move(callback)));
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
        base::BindOnce(&LorgnetteManagerClientImpl::LorgnetteSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    lorgnette_daemon_proxy_->ConnectToSignal(
        lorgnette::kManagerServiceInterface,
        lorgnette::kScannerListChangedSignal,
        base::BindRepeating(
            &LorgnetteManagerClientImpl::ScannerListChangedReceived,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&LorgnetteManagerClientImpl::LorgnetteSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  // Reads scan data on a blocking sequence.
  class ScanDataReader {
   public:
    // In case of success, std::string holds the read data. Otherwise,
    // nullopt.
    using CompletionCallback =
        base::OnceCallback<void(std::optional<std::string> data)>;

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
    void OnDataRead(std::optional<std::string> data) {
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
    std::optional<std::string> data_;

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

  struct DiscoverySessionState {
    std::optional<chromeos::DBusMethodCallback<lorgnette::ListScannersResponse>>
        session_end_callback;
    std::optional<
        base::RepeatingCallback<void(lorgnette::ScannerListChangedSignal)>>
        signal_callback;
    std::string session_id;
    lorgnette::ListScannersResponse response;
    base::TimeTicks last_event;
    base::TimeDelta max_event_interval;
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

    ScanJobState& state = scan_job_state_.begin()->second;
    state.cancel_callback = std::move(cancel_callback);

    CancelScan(request,
               base::BindOnce(&LorgnetteManagerClientImpl::OnCancelScanResponse,
                              weak_ptr_factory_.GetWeakPtr(), uuid));
  }

  // Handles the response received after calling GetScannerCapabilities().
  void OnScannerCapabilitiesResponse(
      chromeos::DBusMethodCallback<lorgnette::ScannerCapabilities> callback,
      dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to obtain ScannerCapabilities";
      std::move(callback).Run(std::nullopt);
      return;
    }

    lorgnette::ScannerCapabilities response_proto;
    dbus::MessageReader reader(response);
    if (!reader.PopArrayOfBytesAsProto(&response_proto)) {
      LOG(ERROR) << "Failed to read ScannerCapabilities";
      std::move(callback).Run(std::nullopt);
      return;
    }

    std::move(callback).Run(std::move(response_proto));
  }

  // Handles the response received after calling OpenScanner().
  void OnOpenScannerResponse(
      chromeos::DBusMethodCallback<lorgnette::OpenScannerResponse> callback,
      dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to obtain OpenScannerResponse";
      std::move(callback).Run(std::nullopt);
      return;
    }

    lorgnette::OpenScannerResponse response_proto;
    dbus::MessageReader reader(response);
    if (!reader.PopArrayOfBytesAsProto(&response_proto)) {
      LOG(ERROR) << "Failed to decode OpenScannerResponse proto";
      std::move(callback).Run(std::nullopt);
      return;
    }

    std::move(callback).Run(response_proto);
  }

  // Handles the response received after calling CloseScanner().
  void OnCloseScannerResponse(
      chromeos::DBusMethodCallback<lorgnette::CloseScannerResponse> callback,
      dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to obtain CloseScannerResponse";
      std::move(callback).Run(std::nullopt);
      return;
    }

    lorgnette::CloseScannerResponse response_proto;
    dbus::MessageReader reader(response);
    if (!reader.PopArrayOfBytesAsProto(&response_proto)) {
      LOG(ERROR) << "Failed to decode CloseScannerResponse proto";
      std::move(callback).Run(std::nullopt);
      return;
    }

    std::move(callback).Run(response_proto);
  }

  // Handles the response received after calling SetOptions().
  void OnSetOptionsResponse(
      chromeos::DBusMethodCallback<lorgnette::SetOptionsResponse> callback,
      dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to obtain SetOptionsResponse";
      std::move(callback).Run(std::nullopt);
      return;
    }

    lorgnette::SetOptionsResponse response_proto;
    dbus::MessageReader reader(response);
    if (!reader.PopArrayOfBytesAsProto(&response_proto)) {
      LOG(ERROR) << "Failed to decode SetOptionsResponse proto";
      std::move(callback).Run(std::nullopt);
      return;
    }

    std::move(callback).Run(response_proto);
  }

  // Handles the response received after calling GetCurrentConfig().
  void OnGetCurrentConfigResponse(
      chromeos::DBusMethodCallback<lorgnette::GetCurrentConfigResponse>
          callback,
      dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to obtain GetCurrentConfigResponse";
      std::move(callback).Run(std::nullopt);
      return;
    }

    lorgnette::GetCurrentConfigResponse response_proto;
    dbus::MessageReader reader(response);
    if (!reader.PopArrayOfBytesAsProto(&response_proto)) {
      LOG(ERROR) << "Failed to decode GetCurrentConfigResponse proto";
      std::move(callback).Run(std::nullopt);
      return;
    }

    std::move(callback).Run(response_proto);
  }

  // Handles the response received after calling StartPreparedScan.
  void OnStartPreparedScanResponse(
      chromeos::DBusMethodCallback<lorgnette::StartPreparedScanResponse>
          callback,
      dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to obtain StartPreparedScanResponse";
      std::move(callback).Run(std::nullopt);
      return;
    }

    lorgnette::StartPreparedScanResponse response_proto;
    dbus::MessageReader reader(response);
    if (!reader.PopArrayOfBytesAsProto(&response_proto)) {
      LOG(ERROR) << "Failed to decode StartPreparedScanResponse proto";
      std::move(callback).Run(std::nullopt);
      return;
    }

    std::move(callback).Run(response_proto);
  }

  // Called when scan data read is completed.
  void OnScanDataCompleted(const std::string& uuid,
                           uint32_t page_number,
                           bool more_pages,
                           std::optional<std::string> data) {
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

  // Handles the response received after calling ReadScanData().
  void OnReadScanDataResponse(
      chromeos::DBusMethodCallback<lorgnette::ReadScanDataResponse> callback,
      dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to obtain ReadScanDataResponse";
      std::move(callback).Run(std::nullopt);
      return;
    }

    lorgnette::ReadScanDataResponse response_proto;
    dbus::MessageReader reader(response);
    if (!reader.PopArrayOfBytesAsProto(&response_proto)) {
      LOG(ERROR) << "Failed to decode ReadScanDataResponse proto";
      std::move(callback).Run(std::nullopt);
      return;
    }

    std::move(callback).Run(response_proto);
  }

  void OnCancelScanResponse(
      const std::string& scan_uuid,
      std::optional<lorgnette::CancelScanResponse> response) {
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

    // If the cancel request failed, report the cancel as failed via the
    // callback. Otherwise, wait for the cancel to complete.
    if (!response->success()) {
      LOG(ERROR) << "Cancelling scan failed: " << response->failure_reason();
      std::move(state.cancel_callback).Run(false);
      return;
    }
  }

  // Handles the response received after calling CancelScan with a
  // CancelScanRequest.
  void OnCancelScanJobResponse(
      chromeos::DBusMethodCallback<lorgnette::CancelScanResponse> callback,
      dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to obtain CancelScanResponse";
      std::move(callback).Run(std::nullopt);
      return;
    }

    lorgnette::CancelScanResponse response_proto;
    dbus::MessageReader reader(response);
    if (!reader.PopArrayOfBytesAsProto(&response_proto)) {
      LOG(ERROR) << "Failed to decode CancelScanResponse proto";
      std::move(callback).Run(std::nullopt);
      return;
    }

    std::move(callback).Run(response_proto);
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

  void OnStartScannerDiscoveryResponse(
      const lorgnette::StartScannerDiscoveryRequest& request,
      base::RepeatingCallback<void(lorgnette::ScannerListChangedSignal)>
          signal_callback,
      chromeos::DBusMethodCallback<lorgnette::StartScannerDiscoveryResponse>
          response_callback,
      dbus::Response* dbus_response) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!dbus_response) {
      LOG(ERROR) << "Failed to obtain StartScannerDiscoveryResponse";
      std::move(response_callback).Run(std::nullopt);
      return;
    }

    lorgnette::StartScannerDiscoveryResponse response;
    dbus::MessageReader reader(dbus_response);
    if (!reader.PopArrayOfBytesAsProto(&response)) {
      LOG(ERROR) << "Failed to decode StartScannerDiscoveryResponse proto";
      std::move(response_callback).Run(std::nullopt);
      return;
    }

    if (!response.started()) {
      LOG(ERROR) << "Scanner discovery session was not started";
      std::move(response_callback).Run(std::nullopt);
      return;
    }

    DiscoverySessionState session;
    session.session_id = response.session_id();
    session.signal_callback = std::move(signal_callback);
    session.last_event = base::TimeTicks::Now();
    session.max_event_interval = base::Milliseconds(0);
    discovery_sessions_[response.session_id()] = std::move(session);
    PRINTER_LOG(DEBUG) << "Client " << request.client_id()
                       << " started discovery session: "
                       << response.session_id();

    if (discovery_sessions_.size() == 1) {
      // Passing "this" is safe because discovery_monitor_ will be destroyed
      // before this object.
      discovery_monitor_.Start(
          FROM_HERE, kDiscoveryMonitorInterval, this,
          &LorgnetteManagerClientImpl::CheckDiscoverySessions);
    }
    std::move(response_callback).Run(std::move(response));
  }

  void CheckDiscoverySessions() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    base::TimeTicks now = base::TimeTicks::Now();
    std::vector<std::string> expired_sessions;
    // Find all of the sessions that should expire due to inactivity.  Delete
    // them in a separate loop because the SESSION_ENDING handler may delete the
    // session from discovery_sessions_.
    for (auto& [id, session] : discovery_sessions_) {
      base::TimeDelta inactive_duration = now - session.last_event;
      if (inactive_duration > kDiscoveryMaxInactivity) {
        expired_sessions.emplace_back(id);
        session.response.set_result(lorgnette::OPERATION_RESULT_CANCELLED);
      }
    }
    for (const std::string& id : expired_sessions) {
      DiscoverySessionState& session = discovery_sessions_.at(id);
      PRINTER_LOG(EVENT) << "Terminating idle discovery session " << id;
      lorgnette::ScannerListChangedSignal signal;
      signal.set_session_id(id);
      signal.set_event_type(
          lorgnette::ScannerListChangedSignal::SESSION_ENDING);
      if (session.signal_callback) {
        session.signal_callback->Run(std::move(signal));
      }
    }
  }

  void OnListScannersDiscoverySession(
      chromeos::DBusMethodCallback<lorgnette::ListScannersResponse> callback,
      std::optional<lorgnette::StartScannerDiscoveryResponse> response) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!response.has_value()) {
      // TODO(b/277049005): Set the proper result code once this is disentangled
      // from the synchronous ListScanners response.
      std::move(callback).Run(std::nullopt);
      return;
    }

    // This will have been created already by OnStartScannerDiscoveryResponse,
    // so no need to search for it first.
    DCHECK(base::Contains(discovery_sessions_, response->session_id()));
    discovery_sessions_[response->session_id()].session_end_callback =
        std::move(callback);
  }

  void OnStopScannerDiscoveryResponse(
      std::string session_id,
      chromeos::DBusMethodCallback<lorgnette::StopScannerDiscoveryResponse>
          callback,
      dbus::Response* dbus_response) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!dbus_response) {
      LOG(ERROR) << "Failed to obtain StopScannerDiscoveryResponse";
      std::move(callback).Run(std::nullopt);
      return;
    }

    lorgnette::StopScannerDiscoveryResponse response;
    dbus::MessageReader reader(dbus_response);
    if (!reader.PopArrayOfBytesAsProto(&response)) {
      LOG(ERROR) << "Failed to decode StopScannerDiscoveryResponse proto";
      std::move(callback).Run(std::nullopt);
      return;
    }

    PRINTER_LOG(DEBUG) << "Scanner discovery session " << session_id
                       << (response.stopped() ? " was " : " was not ")
                       << "stopped.";
    std::move(callback).Run(response);
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

  void ScannerListChangedReceived(dbus::Signal* dbus_signal) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    dbus::MessageReader reader(dbus_signal);
    lorgnette::ScannerListChangedSignal signal;
    if (!reader.PopArrayOfBytesAsProto(&signal)) {
      LOG(ERROR) << "Failed to decode ScannerListChangedSignal proto";
      return;
    }

    if (!base::Contains(discovery_sessions_, signal.session_id())) {
      LOG(ERROR) << "Received signal for unrecognized discovery session: "
                 << signal.session_id();
      return;
    }
    DiscoverySessionState& session = discovery_sessions_[signal.session_id()];

    if (!session.signal_callback) {
      LOG(WARNING) << "Scanner discovery session " << signal.session_id()
                   << " does not have a signal handler registered";
      return;
    }
    session.signal_callback->Run(std::move(signal));
  }

  void ListScannersDiscoveryScannersUpdated(
      lorgnette::ScannerListChangedSignal signal) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(base::Contains(discovery_sessions_, signal.session_id()));
    DiscoverySessionState& session = discovery_sessions_[signal.session_id()];
    base::TimeDelta event_interval =
        base::TimeTicks::Now() - session.last_event;
    if (event_interval > session.max_event_interval) {
      session.max_event_interval = event_interval;
    }

    switch (signal.event_type()) {
      case lorgnette::ScannerListChangedSignal::SCANNER_ADDED:
        PRINTER_LOG(EVENT) << "Discovered SANE scanner: "
                           << signal.scanner().name();
        session.last_event = base::TimeTicks::Now();
        *session.response.add_scanners() = std::move(signal.scanner());
        break;
      case lorgnette::ScannerListChangedSignal::SCANNER_REMOVED:
        // TODO(b/303855027): Once this is implemented in the backend, this
        // needs to be updated to actually remove devices.
        session.last_event = base::TimeTicks::Now();
        break;
      case lorgnette::ScannerListChangedSignal::ENUM_COMPLETE: {
        PRINTER_LOG(EVENT) << "Enumeration completed for discovery session: "
                           << session.session_id;
        session.response.set_result(lorgnette::OPERATION_RESULT_SUCCESS);
        session.last_event = base::TimeTicks::Now();
        lorgnette::StopScannerDiscoveryRequest request;
        request.set_session_id(session.session_id);
        StopScannerDiscovery(request, base::DoNothing());
        break;
      }
      case lorgnette::ScannerListChangedSignal::SESSION_ENDING:
        PRINTER_LOG(EVENT) << "Session ending for discovery session: "
                           << session.session_id;
        base::UmaHistogramCounts100("Scanning.DiscoverySession.NumScanners",
                                    session.response.scanners_size());
        base::UmaHistogramEnumeration(
            "Scanning.DiscoverySession.Result", session.response.result(),
            static_cast<lorgnette::OperationResult>(
                lorgnette::OperationResult_ARRAYSIZE));
        base::UmaHistogramMediumTimes("Scanning.DiscoverySession.MaxInterval",
                                      session.max_event_interval);
        DCHECK(session.session_end_callback);
        std::move(*session.session_end_callback)
            .Run(std::move(session.response));
        discovery_sessions_.erase(session.session_id);
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }

    if (discovery_sessions_.size() == 0) {
      discovery_monitor_.Stop();
    }
  }

  void LorgnetteSignalConnected(const std::string& interface_name,
                                const std::string& signal_name,
                                bool success) {
    LOG_IF(WARNING, !success)
        << "Failed to connect to lorgnette " << interface_name
        << "::" << signal_name << " signal.";
  }

  raw_ptr<dbus::ObjectProxy> lorgnette_daemon_proxy_ = nullptr;

  // Map from scan UUIDs to ScanDataReader and callbacks for reporting scan
  // progress and completion.
  base::flat_map<std::string, ScanJobState> scan_job_state_
      GUARDED_BY_CONTEXT(sequence_checker_);
  // Map from discovery session IDs to a DiscoverySessionState tracking the
  // state and callbacks for that session.
  base::flat_map<std::string, DiscoverySessionState> discovery_sessions_
      GUARDED_BY_CONTEXT(sequence_checker_);
  // Ensures that all callbacks are handled on the same sequence, so that it is
  // safe to access scan_job_state_ and discovery_sessions_ without a lock.
  SEQUENCE_CHECKER(sequence_checker_);
  // Triggers a recurring check for hung discovery sessions when discovery is
  // active.
  base::RepeatingTimer discovery_monitor_;

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
