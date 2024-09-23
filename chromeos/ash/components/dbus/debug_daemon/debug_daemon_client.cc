// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"

#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_config.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "chromeos/ash/components/dbus/debug_daemon/metrics.h"
#include "chromeos/dbus/common/dbus_library_error.h"
#include "chromeos/dbus/common/pipe_reader.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/debugd/dbus-constants.h"

namespace ash {

namespace {

const char kCrOSTracingAgentName[] = "cros";
const char kCrOSTraceLabel[] = "systemTraceEvents";

// Because the cheets logs are very huge, we set the D-Bus timeout to 2 minutes.
const int kBigLogsDBusTimeoutMS = 120 * 1000;

// crash_sender could take a while to run if the network connection is slow, so
// wait up to 20 seconds for it.
const int kCrashSenderTimeoutMS = 20 * 1000;

// NOTE: This does not use the typical pattern of a single `g_instance` variable
// due to browser_tests that need to temporarily override the existing instance
// with a specialized subclass.
DebugDaemonClient* g_instance = nullptr;
DebugDaemonClient* g_instance_for_test = nullptr;

// A self-deleting object that wraps the pipe reader operations for reading the
// big feedback logs. It will delete itself once the pipe stream has been
// terminated. Once the data has been completely read from the pipe, it invokes
// the GetLogsCallback |callback| passing the deserialized logs data back to
// the requester.
class PipeReaderWrapper final {
 public:
  explicit PipeReaderWrapper(DebugDaemonClient::GetLogsCallback callback)
      : pipe_reader_(base::ThreadPool::CreateTaskRunner(
            {base::MayBlock(),
             base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
        callback_(std::move(callback)) {}

  PipeReaderWrapper(const PipeReaderWrapper&) = delete;
  PipeReaderWrapper& operator=(const PipeReaderWrapper&) = delete;

  base::ScopedFD Initialize() {
    return pipe_reader_.StartIO(base::BindOnce(&PipeReaderWrapper::OnIOComplete,
                                               weak_ptr_factory_.GetWeakPtr()));
  }

  void OnIOComplete(std::optional<std::string> result) {
    if (!result.has_value()) {
      VLOG(1) << "Failed to read data.";
      RecordGetFeedbackLogsV2DbusResult(
          GetFeedbackLogsV2DbusResult::kErrorReadingData);
      RunCallbackAndDestroy(std::nullopt);
      return;
    }

    JSONStringValueDeserializer json_reader(result.value());
    std::unique_ptr<base::Value> logs(
        json_reader.Deserialize(nullptr, nullptr));
    if (!logs.get() || !logs->is_dict()) {
      VLOG(1) << "Failed to deserialize the JSON logs.";
      RecordGetFeedbackLogsV2DbusResult(
          GetFeedbackLogsV2DbusResult::kErrorDeserializingJSonLogs);
      RunCallbackAndDestroy(std::nullopt);
      return;
    }
    std::map<std::string, std::string> data;
    for (const auto [dict_key, dict_value] : logs->GetDict()) {
      data[dict_key] = dict_value.GetString();
    }
    RunCallbackAndDestroy(std::move(data));
  }

  void TerminateStream() {
    VLOG(1) << "Terminated";
    RunCallbackAndDestroy(std::nullopt);
  }

  base::WeakPtr<PipeReaderWrapper> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void RunCallbackAndDestroy(
      std::optional<std::map<std::string, std::string>> result) {
    if (result.has_value()) {
      std::move(callback_).Run(true, std::move(result.value()));
    } else {
      std::move(callback_).Run(false, std::map<std::string, std::string>());
    }
    delete this;
  }

  chromeos::PipeReader pipe_reader_;
  DebugDaemonClient::GetLogsCallback callback_;
  base::WeakPtrFactory<PipeReaderWrapper> weak_ptr_factory_{this};
};

// The DebugDaemonClient implementation used in production.
class DebugDaemonClientImpl : public DebugDaemonClient {
 public:
  DebugDaemonClientImpl() : debugdaemon_proxy_(nullptr) {}

  DebugDaemonClientImpl(const DebugDaemonClientImpl&) = delete;
  DebugDaemonClientImpl& operator=(const DebugDaemonClientImpl&) = delete;

  ~DebugDaemonClientImpl() override = default;

  // DebugDaemonClient override.
  void DumpDebugLogs(bool is_compressed,
                     int file_descriptor,
                     chromeos::VoidDBusMethodCallback callback) override {
    // Issue the dbus request to get debug logs.
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kDumpDebugLogs);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(is_compressed);
    writer.AppendFileDescriptor(file_descriptor);
    debugdaemon_proxy_->CallMethod(
        &method_call, kBigLogsDBusTimeoutMS,
        base::BindOnce(&DebugDaemonClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void SetDebugMode(const std::string& subsystem,
                    chromeos::VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kSetDebugMode);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(subsystem);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetRoutes(bool numeric,
                 bool ipv6,
                 bool all_tables,
                 chromeos::DBusMethodCallback<std::vector<std::string>>
                     callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface, debugd::kGetRoutes);
    dbus::MessageWriter writer(&method_call);
    dbus::MessageWriter sub_writer(NULL);
    writer.OpenArray("{sv}", &sub_writer);
    dbus::MessageWriter elem_writer(NULL);
    sub_writer.OpenDictEntry(&elem_writer);
    elem_writer.AppendString("numeric");
    elem_writer.AppendVariantOfBool(numeric);
    sub_writer.CloseContainer(&elem_writer);
    sub_writer.OpenDictEntry(&elem_writer);
    elem_writer.AppendString("v6");
    elem_writer.AppendVariantOfBool(ipv6);
    sub_writer.CloseContainer(&elem_writer);
    sub_writer.OpenDictEntry(&elem_writer);
    elem_writer.AppendString("all");
    elem_writer.AppendVariantOfBool(all_tables);
    sub_writer.CloseContainer(&elem_writer);
    writer.CloseContainer(&sub_writer);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnGetRoutes,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetNetworkStatus(
      chromeos::DBusMethodCallback<std::string> callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kGetNetworkStatus);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnStringMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetNetworkInterfaces(
      chromeos::DBusMethodCallback<std::string> callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kGetInterfaces);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnStringMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetPerfOutput(const std::vector<std::string>& quipper_args,
                     bool disable_cpu_idle,
                     int file_descriptor,
                     chromeos::DBusMethodCallback<uint64_t> callback) override {
    DCHECK(file_descriptor);
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kGetPerfOutputV2);
    dbus::MessageWriter writer(&method_call);
    writer.AppendArrayOfStrings(quipper_args);
    writer.AppendBool(disable_cpu_idle);
    writer.AppendFileDescriptor(file_descriptor);

    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnUint64Method,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void StopPerf(uint64_t session_id,
                chromeos::VoidDBusMethodCallback callback) override {
    DCHECK(session_id);
    dbus::MethodCall method_call(debugd::kDebugdInterface, debugd::kStopPerf);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint64(session_id);

    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetFeedbackLogs(
      const cryptohome::AccountIdentifier& id,
      const std::vector<debugd::FeedbackLogType>& requested_logs,
      GetLogsCallback callback) override {
    // The PipeReaderWrapper is a self-deleting object; we don't have to worry
    // about ownership or lifetime. We need to create a new one for each Big
    // Logs requests in order to queue these requests. One request can take a
    // long time to be processed and a new request should never be ignored nor
    // cancels the on-going one.
    PipeReaderWrapper* pipe_reader = new PipeReaderWrapper(std::move(callback));
    base::ScopedFD pipe_write_end = pipe_reader->Initialize();

    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kGetFeedbackLogsV3);
    dbus::MessageWriter writer(&method_call);
    writer.AppendFileDescriptor(pipe_write_end.get());
    writer.AppendString(id.account_id());
    // Write |requested_logs|.
    dbus::MessageWriter sub_writer(nullptr);
    writer.OpenArray("i", &sub_writer);
    for (auto log_type : requested_logs) {
      sub_writer.AppendInt32(log_type);
    }
    writer.CloseContainer(&sub_writer);

    DVLOG(1) << "Requesting feedback logs";
    debugdaemon_proxy_->CallMethodWithErrorResponse(
        &method_call, kBigLogsDBusTimeoutMS,
        base::BindOnce(&DebugDaemonClientImpl::OnFeedbackLogsResponse,
                       weak_ptr_factory_.GetWeakPtr(),
                       pipe_reader->AsWeakPtr()));
  }

  void GetFeedbackBinaryLogs(
      const cryptohome::AccountIdentifier& id,
      const std::map<debugd::FeedbackBinaryLogType, base::ScopedFD>&
          log_type_fds,
      chromeos::VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kGetFeedbackBinaryLogs);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(id.account_id());

    dbus::MessageWriter array_writer(nullptr);
    // Write map of log_type and fd.
    writer.OpenArray("{ih}", &array_writer);
    for (const auto& log_type : log_type_fds) {
      dbus::MessageWriter dict_entry_writer(nullptr);
      array_writer.OpenDictEntry(&dict_entry_writer);
      dict_entry_writer.AppendInt32(log_type.first);
      dict_entry_writer.AppendFileDescriptor(log_type.second.get());
      array_writer.CloseContainer(&dict_entry_writer);
    }
    writer.CloseContainer(&array_writer);

    DVLOG(1) << "Requesting feedback binary logs";
    debugdaemon_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnFeedbackBinaryLogsResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void BackupArcBugReport(const cryptohome::AccountIdentifier& id,
                          chromeos::VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kBackupArcBugReport);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(id.account_id());

    DVLOG(1) << "Backing up ARC bug report";
    debugdaemon_proxy_->CallMethod(
        &method_call, kBigLogsDBusTimeoutMS,
        base::BindOnce(&DebugDaemonClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetAllLogs(GetLogsCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface, debugd::kGetAllLogs);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnGetAllLogs,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetLog(const std::string& log_name,
              chromeos::DBusMethodCallback<std::string> callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface, debugd::kGetLog);
    dbus::MessageWriter(&method_call).AppendString(log_name);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnStringMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // base::trace_event::TracingAgent implementation.
  std::string GetTracingAgentName() override { return kCrOSTracingAgentName; }

  std::string GetTraceEventLabel() override { return kCrOSTraceLabel; }

  void StartAgentTracing(const base::trace_event::TraceConfig& trace_config,
                         StartAgentTracingCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kSystraceStart);
    dbus::MessageWriter writer(&method_call);
    if (trace_config.systrace_events().empty()) {
      writer.AppendString("all");  // TODO(sleffler) parameterize category list
    } else {
      std::string events;
      for (const std::string& event : trace_config.systrace_events()) {
        if (!events.empty())
          events += " ";
        events += event;
      }
      writer.AppendString(events);
    }

    DVLOG(1) << "Requesting a systrace start";
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnStartMethod,
                       weak_ptr_factory_.GetWeakPtr()));

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), GetTracingAgentName(),
                                  true /* success */));
  }

  void StopAgentTracing(StopAgentTracingCallback callback) override {
    DCHECK(stop_agent_tracing_task_runner_);
    if (pipe_reader_ != NULL) {
      LOG(ERROR) << "Busy doing StopSystemTracing";
      return;
    }

    pipe_reader_ =
        std::make_unique<chromeos::PipeReader>(stop_agent_tracing_task_runner_);
    callback_ = std::move(callback);
    base::ScopedFD pipe_write_end = pipe_reader_->StartIO(base::BindOnce(
        &DebugDaemonClientImpl::OnIOComplete, weak_ptr_factory_.GetWeakPtr()));

    DCHECK(pipe_write_end.is_valid());
    // Issue the dbus request to stop system tracing
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kSystraceStop);
    dbus::MessageWriter writer(&method_call);
    writer.AppendFileDescriptor(pipe_write_end.get());

    DVLOG(1) << "Requesting a systrace stop";
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnStopAgentTracing,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void SetStopAgentTracingTaskRunner(
      scoped_refptr<base::TaskRunner> task_runner) override {
    stop_agent_tracing_task_runner_ = task_runner;
  }

  void TestICMP(const std::string& ip_address,
                TestICMPCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface, debugd::kTestICMP);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(ip_address);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnTestICMP,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void TestICMPWithOptions(const std::string& ip_address,
                           const std::map<std::string, std::string>& options,
                           TestICMPCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kTestICMPWithOptions);
    dbus::MessageWriter writer(&method_call);
    dbus::MessageWriter sub_writer(NULL);
    dbus::MessageWriter elem_writer(NULL);

    // Write the host.
    writer.AppendString(ip_address);

    // Write the options.
    writer.OpenArray("{ss}", &sub_writer);
    std::map<std::string, std::string>::const_iterator it;
    for (it = options.begin(); it != options.end(); ++it) {
      sub_writer.OpenDictEntry(&elem_writer);
      elem_writer.AppendString(it->first);
      elem_writer.AppendString(it->second);
      sub_writer.CloseContainer(&elem_writer);
    }
    writer.CloseContainer(&sub_writer);

    // Call the function.
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnTestICMP,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void UploadCrashes(UploadCrashesCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kUploadCrashes);
    debugdaemon_proxy_->CallMethod(
        &method_call, kCrashSenderTimeoutMS,
        base::BindOnce(&DebugDaemonClientImpl::OnUploadCrashes,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnUploadCrashes(UploadCrashesCallback callback,
                       dbus::Response* response) {
    if (callback.is_null()) {
      return;
    }

    std::move(callback).Run(response != nullptr);
  }

  void EnableDebuggingFeatures(const std::string& password,
                               EnableDebuggingCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kEnableChromeDevFeatures);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(password);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnEnableDebuggingFeatures,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void QueryDebuggingFeatures(QueryDevFeaturesCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kQueryDevFeatures);
    dbus::MessageWriter writer(&method_call);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnQueryDebuggingFeatures,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void RemoveRootfsVerification(EnableDebuggingCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kRemoveRootfsVerification);
    dbus::MessageWriter writer(&method_call);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnRemoveRootfsVerification,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) override {
    debugdaemon_proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

  void SetOomScoreAdj(const std::map<pid_t, int32_t>& pid_to_oom_score_adj,
                      SetOomScoreAdjCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kSetOomScoreAdj);
    dbus::MessageWriter writer(&method_call);

    dbus::MessageWriter sub_writer(nullptr);
    writer.OpenArray("{ii}", &sub_writer);

    dbus::MessageWriter elem_writer(nullptr);
    for (const auto& entry : pid_to_oom_score_adj) {
      sub_writer.OpenDictEntry(&elem_writer);
      elem_writer.AppendInt32(entry.first);
      elem_writer.AppendInt32(entry.second);
      sub_writer.CloseContainer(&elem_writer);
    }
    writer.CloseContainer(&sub_writer);

    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnSetOomScoreAdj,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void CupsAddManuallyConfiguredPrinter(
      const std::string& name,
      const std::string& uri,
      const std::string& language,
      const std::string& ppd_contents,
      DebugDaemonClient::CupsAddPrinterCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kCupsAddManuallyConfiguredPrinterV2);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    writer.AppendString(uri);
    writer.AppendString(language);
    writer.AppendArrayOfBytes(base::as_byte_span(ppd_contents));

    debugdaemon_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnPrinterAdded,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void CupsAddAutoConfiguredPrinter(
      const std::string& name,
      const std::string& uri,
      const std::string& language,
      DebugDaemonClient::CupsAddPrinterCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kCupsAddAutoConfiguredPrinterV2);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    writer.AppendString(uri);
    writer.AppendString(language);

    debugdaemon_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnPrinterAdded,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void CupsRemovePrinter(const std::string& name,
                         DebugDaemonClient::CupsRemovePrinterCallback callback,
                         base::OnceClosure error_callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kCupsRemovePrinter);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);

    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnPrinterRemoved,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       std::move(error_callback)));
  }

  void CupsRetrievePrinterPpd(
      const std::string& name,
      DebugDaemonClient::CupsRetrievePrinterPpdCallback callback,
      base::OnceClosure error_callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kCupsRetrievePpd);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);

    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnRetrievedPrinterPpd,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       std::move(error_callback)));
  }

  void StartPluginVmDispatcher(const std::string& owner_id,
                               const std::string& lang,
                               PluginVmDispatcherCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kStartVmPluginDispatcher);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(owner_id);
    writer.AppendString(lang);
    debugdaemon_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnStartPluginVmDispatcher,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void StopPluginVmDispatcher(PluginVmDispatcherCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kStopVmPluginDispatcher);
    dbus::MessageWriter writer(&method_call);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnStopPluginVmDispatcher,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void SetRlzPingSent(SetRlzPingSentCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kSetRlzPingSent);
    dbus::MessageWriter writer(&method_call);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnSetRlzPingSent,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void SetKstaledRatio(uint8_t val, KstaledRatioCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kKstaledSetRatio);
    dbus::MessageWriter writer(&method_call);
    writer.AppendByte(val);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnSetKstaledRatio,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void SetSchedulerConfigurationV2(
      const std::string& config_name,
      bool lock_policy,
      SetSchedulerConfigurationV2Callback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kSetSchedulerConfigurationV2);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(config_name);
    writer.AppendBool(lock_policy);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnSetSchedulerConfigurationV2,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void SetU2fFlags(const std::set<std::string>& flags,
                   chromeos::VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kSetU2fFlags);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(base::JoinString(
        std::vector<std::string>(flags.begin(), flags.end()), ","));
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetU2fFlags(
      chromeos::DBusMethodCallback<std::set<std::string>> callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kGetU2fFlags);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnGetU2fFlags,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void BluetoothStartBtsnoop(BluetoothBtsnoopCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kBluetoothStartBtsnoop);
    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnBluetoothStartBtsnoop,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void BluetoothStopBtsnoop(int fd,
                            BluetoothBtsnoopCallback callback) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kBluetoothStopBtsnoop);
    dbus::MessageWriter writer(&method_call);
    writer.AppendFileDescriptor(fd);

    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnBluetoothStopBtsnoop,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void StopPacketCapture(const std::string& handle) override {
    dbus::MethodCall method_call(debugd::kDebugdInterface,
                                 debugd::kPacketCaptureStop);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(handle);

    debugdaemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DebugDaemonClientImpl::OnStopMethod,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // DebugDaemonClient Observer overrides.
  void AddObserver(Observer* observer) override {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  void Init(dbus::Bus* bus) override {
    debugdaemon_proxy_ =
        bus->GetObjectProxy(debugd::kDebugdServiceName,
                            dbus::ObjectPath(debugd::kDebugdServicePath));
    // Listen to D-Bus signals emitted by debugd.
    auto on_connected_callback =
        base::BindRepeating(&DebugDaemonClientImpl::SignalConnected,
                            weak_ptr_factory_.GetWeakPtr());
    debugdaemon_proxy_->ConnectToSignal(
        debugd::kDebugdInterface, debugd::kPacketCaptureStartSignal,
        base::BindRepeating(
            &DebugDaemonClientImpl::PacketCaptureStartSignalReceived,
            weak_ptr_factory_.GetWeakPtr()),
        on_connected_callback);
    debugdaemon_proxy_->ConnectToSignal(
        debugd::kDebugdInterface, debugd::kPacketCaptureStopSignal,
        base::BindRepeating(
            &DebugDaemonClientImpl::PacketCaptureStopSignalReceived,
            weak_ptr_factory_.GetWeakPtr()),
        on_connected_callback);
  }

 private:
  void OnGetRoutes(
      chromeos::DBusMethodCallback<std::vector<std::string>> callback,
      dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(std::nullopt);
      return;
    }

    std::vector<std::string> routes;
    dbus::MessageReader reader(response);
    if (!reader.PopArrayOfStrings(&routes)) {
      LOG(ERROR) << "Got non-array response from GetRoutes";
      std::move(callback).Run(std::nullopt);
      return;
    }

    std::move(callback).Run(std::move(routes));
  }

  void OnGetAllLogs(GetLogsCallback callback, dbus::Response* response) {
    std::map<std::string, std::string> logs;
    bool broken = false;  // did we see a broken (k,v) pair?
    dbus::MessageReader sub_reader(NULL);
    if (!response || !dbus::MessageReader(response).PopArray(&sub_reader)) {
      std::move(callback).Run(false, logs);
      return;
    }
    while (sub_reader.HasMoreData()) {
      dbus::MessageReader sub_sub_reader(NULL);
      std::string key, value;
      if (!sub_reader.PopDictEntry(&sub_sub_reader) ||
          !sub_sub_reader.PopString(&key) ||
          !sub_sub_reader.PopString(&value)) {
        broken = true;
        break;
      }
      logs[key] = value;
    }
    std::move(callback).Run(!sub_reader.HasMoreData() && !broken, logs);
  }

  void OnFeedbackLogsResponse(base::WeakPtr<PipeReaderWrapper> pipe_reader,
                              dbus::Response* response,
                              dbus::ErrorResponse* err_response) {
    RecordGetFeedbackLogsV2DbusError(err_response);
    if (!response && pipe_reader.get()) {
      // We need to terminate the data stream if an error occurred while the
      // pipe reader is still waiting on read.
      pipe_reader->TerminateStream();
    }
  }

  void OnFeedbackBinaryLogsResponse(chromeos::VoidDBusMethodCallback callback,
                                    dbus::Response* response,
                                    dbus::ErrorResponse* err_response) {
    bool succeeded = !err_response;
    if (!succeeded) {
      LOG(ERROR) << "Failed to GetFeedbackBinaryLogs. Error: "
                 << err_response->GetErrorName();
    }
    std::move(callback).Run(succeeded);
  }

  // Called when a response for a simple start is received.
  void OnStartMethod(dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to request start";
      return;
    }
  }

  // Called when a response for a simple stop is received.
  void OnStopMethod(dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to request stop method through D-Bus";
      return;
    }
  }

  // Called when D-Bus method call which does not return the result is
  // completed or on its error.
  void OnVoidMethod(chromeos::VoidDBusMethodCallback callback,
                    dbus::Response* response) {
    std::move(callback).Run(response);
  }

  void OnUint64Method(chromeos::DBusMethodCallback<uint64_t> callback,
                      dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(std::nullopt);
      return;
    }

    dbus::MessageReader reader(response);
    uint64_t result;
    if (!reader.PopUint64(&result)) {
      std::move(callback).Run(std::nullopt);
      return;
    }

    std::move(callback).Run(std::move(result));
  }

  // Called when D-Bus method call which returns a string is completed or on
  // its error.
  void OnStringMethod(chromeos::DBusMethodCallback<std::string> callback,
                      dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(std::nullopt);
      return;
    }

    dbus::MessageReader reader(response);
    std::string result;
    if (!reader.PopString(&result)) {
      std::move(callback).Run(std::nullopt);
      return;
    }

    std::move(callback).Run(std::move(result));
  }

  void OnEnableDebuggingFeatures(EnableDebuggingCallback callback,
                                 dbus::Response* response) {
    if (callback.is_null())
      return;

    std::move(callback).Run(response != nullptr);
  }

  void OnQueryDebuggingFeatures(QueryDevFeaturesCallback callback,
                                dbus::Response* response) {
    if (callback.is_null())
      return;

    int32_t feature_mask = DEV_FEATURE_NONE;
    if (!response || !dbus::MessageReader(response).PopInt32(&feature_mask)) {
      std::move(callback).Run(false,
                              debugd::DevFeatureFlag::DEV_FEATURES_DISABLED);
      return;
    }

    std::move(callback).Run(true, feature_mask);
  }

  void OnRemoveRootfsVerification(EnableDebuggingCallback callback,
                                  dbus::Response* response) {
    if (callback.is_null())
      return;

    std::move(callback).Run(response != nullptr);
  }

  // Called when a response for StopAgentTracing() is received.
  void OnStopAgentTracing(dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to request systrace stop";
      // If debugd crashes or completes I/O before this message is processed
      // then pipe_reader_ can be NULL, see OnIOComplete().
      if (pipe_reader_.get()) {
        pipe_reader_.reset();
        std::move(callback_).Run(GetTracingAgentName(), GetTraceEventLabel(),
                                 scoped_refptr<base::RefCountedString>(
                                     new base::RefCountedString()));
      }
    }
    // NB: requester is signaled when i/o completes
  }

  void OnTestICMP(TestICMPCallback callback, dbus::Response* response) {
    std::string status;
    if (!response || !dbus::MessageReader(response).PopString(&status)) {
      std::move(callback).Run(std::nullopt);
      return;
    }

    std::move(callback).Run(status);
  }

  // Called when pipe i/o completes; pass data on and delete the instance.
  void OnIOComplete(std::optional<std::string> result) {
    pipe_reader_.reset();
    std::string pipe_data =
        result.has_value() ? std::move(result).value() : std::string();
    std::move(callback_).Run(
        GetTracingAgentName(), GetTraceEventLabel(),
        base::MakeRefCounted<base::RefCountedString>(std::move(pipe_data)));
  }

  void OnSetOomScoreAdj(SetOomScoreAdjCallback callback,
                        dbus::Response* response) {
    std::string output;
    if (response && dbus::MessageReader(response).PopString(&output))
      std::move(callback).Run(true, output);
    else
      std::move(callback).Run(false, "");
  }

  void OnPrinterAdded(CupsAddPrinterCallback callback,
                      dbus::Response* response,
                      dbus::ErrorResponse* err_response) {
    int32_t result;

    // If we get a normal response, we need not examine the error response.
    if (response && dbus::MessageReader(response).PopInt32(&result)) {
      DCHECK_GE(result, 0);
      std::move(callback).Run(result);
      return;
    }

    // Without a normal response, we communicate the D-Bus error response
    // to the callback.
    std::string err_str;
    if (err_response) {
      dbus::MessageReader err_reader(err_response);
      err_str = err_response->GetErrorName();
    }
    chromeos::DBusLibraryError dbus_error =
        chromeos::DBusLibraryErrorFromString(err_str);
    std::move(callback).Run(dbus_error);
  }

  void OnPrinterRemoved(CupsRemovePrinterCallback callback,
                        base::OnceClosure error_callback,
                        dbus::Response* response) {
    bool result = false;
    if (response && dbus::MessageReader(response).PopBool(&result))
      std::move(callback).Run(result);
    else
      std::move(error_callback).Run();
  }

  void OnRetrievedPrinterPpd(CupsRetrievePrinterPpdCallback callback,
                             base::OnceClosure error_callback,
                             dbus::Response* response) {
    size_t length = 0;
    const uint8_t* bytes = nullptr;

    if (!(response &&
          dbus::MessageReader(response).PopArrayOfBytes(&bytes, &length)) ||
        length == 0 || bytes == nullptr) {
      LOG(ERROR) << "Failed to retrieve printer PPD";
      std::move(error_callback).Run();
      return;
    }

    std::vector<uint8_t> data(bytes, bytes + length);
    std::move(callback).Run(data);
  }

  void OnStartPluginVmDispatcher(PluginVmDispatcherCallback callback,
                                 dbus::Response* response,
                                 dbus::ErrorResponse* error) {
    if (error) {
      LOG(ERROR) << "Failed to start dispatcher, DBus error "
                 << error->GetErrorName();
      std::move(callback).Run(false);
      return;
    }

    bool result = false;
    if (response) {
      dbus::MessageReader reader(response);
      reader.PopBool(&result);
    }
    std::move(callback).Run(result);
  }

  void OnStopPluginVmDispatcher(PluginVmDispatcherCallback callback,
                                dbus::Response* response) {
    // Debugd just sends back an empty response, so we just check if
    // the response exists
    std::move(callback).Run(response != nullptr);
  }

  void OnSetRlzPingSent(SetRlzPingSentCallback callback,
                        dbus::Response* response) {
    bool result = false;
    if (response) {
      dbus::MessageReader reader(response);
      reader.PopBool(&result);
    }
    std::move(callback).Run(result);
  }

  void OnSetKstaledRatio(KstaledRatioCallback callback,
                         dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to read debugd response";
      std::move(callback).Run(false);
      return;
    }

    bool result = false;
    dbus::MessageReader reader(response);
    if (!reader.PopBool(&result)) {
      LOG(ERROR) << "Debugd response did not contain a bool";
      std::move(callback).Run(false);
      return;
    }

    std::move(callback).Run(result);
  }

  void OnSetSchedulerConfigurationV2(
      SetSchedulerConfigurationV2Callback callback,
      dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(false, 0);
      return;
    }

    bool result = false;
    uint32_t num_cores_disabled = 0;
    dbus::MessageReader reader(response);
    if (!reader.PopBool(&result) || !reader.PopUint32(&num_cores_disabled)) {
      LOG(ERROR) << "Failed to read SetSchedulerConfigurationV2 response";
      std::move(callback).Run(false, 0);
      return;
    }

    std::move(callback).Run(result, num_cores_disabled);
  }

  void OnGetU2fFlags(
      chromeos::DBusMethodCallback<std::set<std::string>> callback,
      dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(std::nullopt);
      return;
    }

    std::string flags_string;
    dbus::MessageReader reader(response);
    if (!reader.PopString(&flags_string)) {
      LOG(ERROR) << "Failed to read GetU2fFlags response";
      std::move(callback).Run(std::nullopt);
      return;
    }

    std::set<std::string> flags;
    for (const auto& flag :
         base::SplitString(flags_string, ",", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY)) {
      flags.insert(flag);
    }

    std::move(callback).Run(std::move(flags));
  }

  void OnBluetoothStartBtsnoop(BluetoothBtsnoopCallback callback,
                               dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to read debugd response";
    }
    std::move(callback).Run(response != nullptr);
  }

  void OnBluetoothStopBtsnoop(BluetoothBtsnoopCallback callback,
                              dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to read debugd response";
    }
    std::move(callback).Run(response != nullptr);
  }

  // Called when a D-Bus signal is initially connected.
  void SignalConnected(const std::string& interface_name,
                       const std::string& signal_name,
                       bool success) {
    if (!success)
      LOG(ERROR) << "Failed to connect to signal " << signal_name << ".";
  }

  void PacketCaptureStartSignalReceived(dbus::Signal* signal) override {
    for (auto& observer : observers_)
      observer.OnPacketCaptureStarted();
  }

  void PacketCaptureStopSignalReceived(dbus::Signal* signal) override {
    for (auto& observer : observers_)
      observer.OnPacketCaptureStopped();
  }

  raw_ptr<dbus::ObjectProxy> debugdaemon_proxy_;
  std::unique_ptr<chromeos::PipeReader> pipe_reader_;
  StopAgentTracingCallback callback_;
  scoped_refptr<base::TaskRunner> stop_agent_tracing_task_runner_;
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<DebugDaemonClientImpl> weak_ptr_factory_{this};
};

}  // namespace

// static
DebugDaemonClient* DebugDaemonClient::Get() {
  if (g_instance_for_test)
    return g_instance_for_test;
  return g_instance;
}

// static
void DebugDaemonClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  CHECK(!g_instance);
  g_instance = new DebugDaemonClientImpl();
  g_instance->Init(bus);
}

// static
void DebugDaemonClient::InitializeFake() {
  CHECK(!g_instance);
  g_instance = new FakeDebugDaemonClient();
  g_instance->Init(nullptr);
}

// static
void DebugDaemonClient::SetInstanceForTest(DebugDaemonClient* client) {
  g_instance_for_test = client;
}

// static
void DebugDaemonClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
  g_instance = nullptr;
}

DebugDaemonClient::DebugDaemonClient() = default;

DebugDaemonClient::~DebugDaemonClient() = default;

// static
std::unique_ptr<DebugDaemonClient> DebugDaemonClient::CreateInstance() {
  return std::make_unique<DebugDaemonClientImpl>();
}

}  // namespace ash
