// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/missive/missive_client.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chromeos/dbus/missive/fake_missive_client.h"
#include "chromeos/dbus/missive/history_tracker.h"
#include "components/reporting/proto/synced/interface.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/disconnectable_client.h"
#include "components/reporting/util/status.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "google_apis/google_api_keys.h"
#include "third_party/cros_system_api/dbus/missive/dbus-constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::reporting::Priority;
using ::reporting::Record;
using ::reporting::SequenceInformation;
using ::reporting::SignedEncryptionInfo;
using ::reporting::Status;

namespace chromeos {

// This feature enables retrying enqueueing records if dBus fails. The number of
// retries is controlled by the `kNumSecondsToRetry` parameter. Enabled by
// default because this is a bug fix. Only putting it behind a feature flag for
// kill switch in case of emergency.
// TODO(b/339059662): remove feature flag once retries are in stable channel.
BASE_FEATURE(kEnableRetryEnqueueRecord,
             "EnableRetryEnqueueRecord",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Number of seconds we'll retry to enqueue a record if Missive is unavailable.
// If `kEnableRetryEnqueueRecord` is not enabled, the parameter is not set, or
// set to an invalid int value, then Get() will return the default value.
// TODO(b/339059662): remove feature parameter once retries are in stable
// channel.
const base::FeatureParam<int> kNumSecondsToRetry{
    &kEnableRetryEnqueueRecord, "num_seconds_to_retry",
    /*default seconds to retry=*/2};
namespace {

constexpr char kUmaMissiveClientDbusError[] =
    "Browser.ERP.MissiveClientDbusError";

constexpr char kUmaRetryEnqueueRecordStatus[] =
    "Browser.ERP.RetryEnqueueRecordStatus";

constexpr char kUmaTimeSpentRetryingEnqueueRecord[] =
    "Browser.ERP.TimeSpentRetryingEnqueueRecord";

constexpr char kErrorNoDbusResponse[] = "Returned no response";

MissiveClient* g_instance = nullptr;

// Returns `false` if the api_key is empty or known to be used for testing
// purposes, or by devices that are running unofficial builds.
bool IsApiKeyAccepted(std::string_view api_key) {
  static constexpr std::string_view kBlockListedKeys[] = {
      "dummykey", "dummytoken",
      // More keys or key fragments can be added.
  };
  if (api_key.empty()) {
    LOG(ERROR) << "API Key is empty";
    return false;
  }
  const std::string lowercase_api_key = base::ToLowerASCII(api_key);
  for (auto key : kBlockListedKeys) {
    if (base::Contains(lowercase_api_key, key)) {
      LOG(ERROR) << "API Key is block-listed: " << api_key;
      return false;
    }
  }
  return true;
}

class MissiveClientImpl : public MissiveClient {
 public:
  MissiveClientImpl() : client_(origin_task_runner()) {}
  MissiveClientImpl(const MissiveClientImpl& other) = delete;
  MissiveClientImpl& operator=(const MissiveClientImpl& other) = delete;
  ~MissiveClientImpl() override = default;

  void Init(dbus::Bus* const bus) {
    DCHECK(bus);
    origin_task_runner_ = bus->GetOriginTaskRunner();

    // Never changes after this moment.
    has_valid_api_key_ = IsApiKeyAccepted(google_apis::GetAPIKey());

    // Never changes back to `false`.
    is_initialized_ = true;

    DCHECK(!missive_service_proxy_);
    missive_service_proxy_ =
        bus->GetObjectProxy(missive::kMissiveServiceName,
                            dbus::ObjectPath(missive::kMissiveServicePath));
    missive_service_proxy_->SetNameOwnerChangedCallback(base::BindRepeating(
        &MissiveClientImpl::OwnerChanged, weak_ptr_factory_.GetWeakPtr()));
    missive_service_proxy_->WaitForServiceToBeAvailable(base::BindOnce(
        &MissiveClientImpl::ServiceAvailable, weak_ptr_factory_.GetWeakPtr()));
  }

  void MaybeRetryEnqueue(
      bool is_retry,
      base::TimeTicks time_record_was_enqueued,
      const reporting::Priority priority,
      reporting::Record record,
      base::OnceCallback<void(reporting::Status)> completion_callback,
      reporting::Status status) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(origin_checker_);

    const base::TimeDelta time_elapased_since_record_was_originally_enqueued =
        base::TimeTicks::Now() - time_record_was_enqueued;

    const base::TimeDelta retry_window =
        base::Seconds(kNumSecondsToRetry.Get());

    // If missive is unavailable and we're within the retry window, retry
    // enqueueuing the record.
    if (time_elapased_since_record_was_originally_enqueued < retry_window &&
        status.error_code() == reporting::error::UNAVAILABLE) {
      EnqueueRecordInternal(/*is_retry=*/true, time_record_was_enqueued,
                            priority, std::move(record),
                            std::move(completion_callback));
      return;
    }

    if (is_retry) {
      base::UmaHistogramTimes(
          kUmaTimeSpentRetryingEnqueueRecord,
          time_elapased_since_record_was_originally_enqueued);
      base::UmaHistogramEnumeration(kUmaRetryEnqueueRecordStatus, status.code(),
                                    reporting::error::Code::MAX_VALUE);
    }
    std::move(completion_callback).Run(status);
  }

  void EnqueueRecordInternal(
      bool is_retry,
      base::TimeTicks time_record_was_enqueued,
      const reporting::Priority priority,
      reporting::Record record,
      base::OnceCallback<void(reporting::Status)> completion_callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(origin_checker_);

    base::OnceCallback<void(reporting::Status)> maybe_retry_enqueue_cb;

    if (base::FeatureList::IsEnabled(kEnableRetryEnqueueRecord)) {
      // Make a copy of the record for the retry callback.
      reporting::Record record_copy(record);

      maybe_retry_enqueue_cb = base::BindPostTask(
          origin_task_runner_,
          base::BindOnce(
              &MissiveClientImpl::MaybeRetryEnqueue,
              weak_ptr_factory_.GetWeakPtr(), is_retry,
              time_record_was_enqueued, priority, std::move(record_copy),
              reporting::Scoped<reporting::Status>(
                  std::move(completion_callback),
                  reporting::Status(reporting::error::UNAVAILABLE,
                                    "Missive client destructed before "
                                    "record was enqueued"))));
    } else {
      maybe_retry_enqueue_cb = std::move(completion_callback);
    }

    auto delegate = std::make_unique<EnqueueRecordDelegate>(
        priority, std::move(record), this, std::move(maybe_retry_enqueue_cb));

    client_.MaybeMakeCall(std::move(delegate));
  }

  void EnqueueRecord(const reporting::Priority priority,
                     reporting::Record record,
                     base::OnceCallback<void(reporting::Status)>
                         completion_callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(origin_checker_);
    if (!is_initialized_) {
      std::move(completion_callback)
          .Run(reporting::Status(reporting::error::FAILED_PRECONDITION,
                                 "Reporting not started yet"));
      return;
    }
    if (!has_valid_api_key()) {
      std::move(completion_callback)
          .Run(reporting::Status(reporting::error::FAILED_PRECONDITION,
                                 "Cannot report with unsupported API Key"));
      return;
    }

    EnqueueRecordInternal(
        /*is_retry=*/false, /*time_record_was_enqueued=*/base::TimeTicks::Now(),
        priority, std::move(record), std::move(completion_callback));
  }

  void Flush(const reporting::Priority priority,
             base::OnceCallback<void(reporting::Status)> completion_callback)
      override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(origin_checker_);
    if (!is_initialized_) {
      std::move(completion_callback)
          .Run(reporting::Status(reporting::error::FAILED_PRECONDITION,
                                 "Reporting not started yet"));
      return;
    }
    if (!has_valid_api_key()) {
      std::move(completion_callback)
          .Run(reporting::Status(reporting::error::FAILED_PRECONDITION,
                                 "Cannot report with unsupported API Key"));
      return;
    }
    auto delegate = std::make_unique<FlushDelegate>(
        priority, this, std::move(completion_callback));
    client_.MaybeMakeCall(std::move(delegate));
  }

  void UpdateConfigInMissive(
      const reporting::ListOfBlockedDestinations& destinations) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(origin_checker_);
    if (!is_initialized_ || !has_valid_api_key()) {
      return;
    }
    auto delegate =
        std::make_unique<UpdateConfigInMissiveDelegate>(destinations, this);
    client_.MaybeMakeCall(std::move(delegate));
  }

  void UpdateEncryptionKey(
      const reporting::SignedEncryptionInfo& encryption_info) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(origin_checker_);
    if (!is_initialized_ || !has_valid_api_key()) {
      return;
    }
    auto delegate =
        std::make_unique<UpdateEncryptionKeyDelegate>(encryption_info, this);
    client_.MaybeMakeCall(std::move(delegate));
  }

  void ReportSuccess(const reporting::SequenceInformation& sequence_information,
                     bool force_confirm) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(origin_checker_);
    if (!is_initialized_ || !has_valid_api_key()) {
      return;
    }
    auto delegate = std::make_unique<ReportSuccessDelegate>(
        sequence_information, force_confirm, this);
    client_.MaybeMakeCall(std::move(delegate));
  }

  MissiveClient::TestInterface* GetTestInterface() override { return nullptr; }

  base::WeakPtr<MissiveClient> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // Class implements DisconnectableClient::Delegate specifically for dBus
  // calls. Logic that handles dBus connect/disconnect cases remains with the
  // base class.
  class DBusDelegate : public reporting::DisconnectableClient::Delegate {
   public:
    // If this enum is changed, be sure to update
    // `EnterpriseReportingMissiveClientDbusError` in
    // tools/metrics/histograms/metadata/browser/enums.xml
    enum DbusErrorType : int32_t {
      OK = 0,
      SERVICE_UNAVAILABLE = 1,
      NO_RESPONSE = 2,
      UNKNOWN = 3,
      MAX_VALUE = UNKNOWN,
    };
    DBusDelegate(const DBusDelegate& other) = delete;
    DBusDelegate& operator=(const DBusDelegate& other) = delete;
    ~DBusDelegate() override = default;

    // Writes request into dBus message writer.
    virtual bool WriteRequest(dbus::MessageWriter* writer) = 0;

    // Parses response, retrieves status information from it and returns it.
    // Optional - returns OK if absent.
    virtual reporting::Status ParseResponse(dbus::MessageReader* reader) {
      return reporting::Status::StatusOK();
    }

   protected:
    DBusDelegate(
        const char* dbus_method,
        MissiveClientImpl* owner,
        base::OnceCallback<void(reporting::Status)> completion_callback)
        : dbus_method_(dbus_method),
          owner_(owner),
          completion_callback_(std::move(completion_callback)) {}

   private:
    // Implementation of DisconnectableClient::Delegate.
    void DoCall(base::OnceClosure cb) final {
      DCHECK_CALLED_ON_VALID_SEQUENCE(owner_->origin_checker_);
      base::ScopedClosureRunner autorun(std::move(cb));
      dbus::MethodCall method_call(missive::kMissiveServiceInterface,
                                   dbus_method_);
      dbus::MessageWriter writer(&method_call);
      if (!WriteRequest(&writer)) {
        reporting::Status status(
            reporting::error::UNKNOWN,
            "MessageWriter was unable to append the request.");
        LOG(ERROR) << status;
        std::move(completion_callback_).Run(status);
        return;
      }

      // Make a dBus call.
      owner_->missive_service_proxy_->CallMethod(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
          base::BindOnce(
              [](base::ScopedClosureRunner autorun,
                 base::WeakPtr<DBusDelegate> self, dbus::Response* response) {
                if (!self) {
                  return;  // Delegate already deleted.
                }
                DCHECK_CALLED_ON_VALID_SEQUENCE(self->owner_->origin_checker_);
                if (!response) {
                  self->Respond(Status(reporting::error::UNAVAILABLE,
                                       kErrorNoDbusResponse));
                  return;
                }
                self->response_ = response;
              },
              std::move(autorun), weak_ptr_factory_.GetWeakPtr()));
    }

    // Process dBus response, if status is OK, or error otherwise.
    void Respond(reporting::Status status) final {
      DCHECK_CALLED_ON_VALID_SEQUENCE(owner_->origin_checker_);
      if (!completion_callback_) {
        return;
      }
      if (status.ok()) {
        dbus::MessageReader reader(response_);
        status = ParseResponse(&reader);
        base::UmaHistogramEnumeration(kUmaMissiveClientDbusError,
                                      DbusErrorType::OK,
                                      DbusErrorType::MAX_VALUE);
      } else if (status.error_message() ==
                 reporting::disconnectable_client::kErrorServiceUnavailable) {
        base::UmaHistogramEnumeration(kUmaMissiveClientDbusError,
                                      DbusErrorType::SERVICE_UNAVAILABLE,
                                      DbusErrorType::MAX_VALUE);
      } else if (status.error_message() == kErrorNoDbusResponse) {
        base::UmaHistogramEnumeration(kUmaMissiveClientDbusError,
                                      DbusErrorType::NO_RESPONSE,
                                      DbusErrorType::MAX_VALUE);
      } else {
        base::UmaHistogramEnumeration(kUmaMissiveClientDbusError,
                                      DbusErrorType::UNKNOWN,
                                      DbusErrorType::MAX_VALUE);
      }
      std::move(completion_callback_).Run(status);
    }

    const char* const dbus_method_;
    raw_ptr<dbus::Response> response_;
    const raw_ptr<MissiveClientImpl> owner_;
    base::OnceCallback<void(reporting::Status)> completion_callback_;

    // Weak pointer factory - must be last member of the class.
    base::WeakPtrFactory<DBusDelegate> weak_ptr_factory_{this};
  };

  class EnqueueRecordDelegate : public DBusDelegate {
   public:
    EnqueueRecordDelegate(
        reporting::Priority priority,
        reporting::Record record,
        MissiveClientImpl* owner,
        base::OnceCallback<void(reporting::Status)> completion_callback)
        : DBusDelegate(missive::kEnqueueRecord,
                       owner,
                       std::move(completion_callback)) {
      *request_.mutable_record() = std::move(record);
      request_.set_priority(priority);

#if BUILDFLAG(IS_CHROMEOS_ASH)
      // Turn on/off the debug state flag (for Ash only).
      request_.set_health_data_logging_enabled(
          ::reporting::HistoryTracker::Get()->debug_state());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    }

    bool WriteRequest(dbus::MessageWriter* writer) override {
      return writer->AppendProtoAsArrayOfBytes(request_);
    }

    reporting::Status ParseResponse(dbus::MessageReader* reader) override {
      reporting::EnqueueRecordResponse response_body;
      if (!reader->PopArrayOfBytesAsProto(&response_body)) {
        return reporting::Status(reporting::error::INTERNAL,
                                 "Response was not parsable.");
      }

#if BUILDFLAG(IS_CHROMEOS_ASH)
      // Accept health data if present (ChromeOS only)
      if (response_body.has_health_data()) {
        ::reporting::HistoryTracker::Get()->set_data(
            std::move(response_body.health_data()), base::DoNothing());
      }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

      reporting::Status status;
      status.RestoreFrom(response_body.status());
      return status;
    }

   private:
    reporting::EnqueueRecordRequest request_;
  };

  class FlushDelegate : public DBusDelegate {
   public:
    FlushDelegate(
        reporting::Priority priority,
        MissiveClientImpl* owner,
        base::OnceCallback<void(reporting::Status)> completion_callback)
        : DBusDelegate(missive::kFlushPriority,
                       owner,
                       std::move(completion_callback)) {
      request_.set_priority(priority);

#if BUILDFLAG(IS_CHROMEOS_ASH)
      // Turn on/off the debug state flag (for Ash only).
      request_.set_health_data_logging_enabled(
          ::reporting::HistoryTracker::Get()->debug_state());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    }

    bool WriteRequest(dbus::MessageWriter* writer) override {
      return writer->AppendProtoAsArrayOfBytes(request_);
    }

    reporting::Status ParseResponse(dbus::MessageReader* reader) override {
      reporting::FlushPriorityResponse response_body;
      if (!reader->PopArrayOfBytesAsProto(&response_body)) {
        return reporting::Status(reporting::error::INTERNAL,
                                 "Response was not parsable.");
      }

#if BUILDFLAG(IS_CHROMEOS_ASH)
      // Accept health data if present (ChromeOS only)
      if (response_body.has_health_data()) {
        ::reporting::HistoryTracker::Get()->set_data(
            std::move(response_body.health_data()), base::DoNothing());
      }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

      reporting::Status status;
      status.RestoreFrom(response_body.status());
      return status;
    }

   private:
    reporting::FlushPriorityRequest request_;
  };

  class UpdateConfigInMissiveDelegate : public DBusDelegate {
   public:
    UpdateConfigInMissiveDelegate(
        const reporting::ListOfBlockedDestinations& destinations,
        MissiveClientImpl* owner)
        : DBusDelegate(missive::kUpdateConfigInMissive,
                       owner,
                       base::DoNothing()) {
      *request_.mutable_list_of_blocked_destinations() = destinations;
    }

    bool WriteRequest(dbus::MessageWriter* writer) override {
      return writer->AppendProtoAsArrayOfBytes(request_);
    }

   private:
    reporting::UpdateConfigInMissiveRequest request_;
  };

  class UpdateEncryptionKeyDelegate : public DBusDelegate {
   public:
    UpdateEncryptionKeyDelegate(
        const reporting::SignedEncryptionInfo& encryption_info,
        MissiveClientImpl* owner)
        : DBusDelegate(missive::kUpdateEncryptionKey,
                       owner,
                       base::DoNothing()) {
      *request_.mutable_signed_encryption_info() = encryption_info;
    }

    bool WriteRequest(dbus::MessageWriter* writer) override {
      return writer->AppendProtoAsArrayOfBytes(request_);
    }

   private:
    reporting::UpdateEncryptionKeyRequest request_;
  };

  class ReportSuccessDelegate : public DBusDelegate {
   public:
    ReportSuccessDelegate(
        const reporting::SequenceInformation& sequence_information,
        bool force_confirm,
        MissiveClientImpl* owner)
        : DBusDelegate(missive::kConfirmRecordUpload,
                       owner,
                       base::DoNothing()) {
      *request_.mutable_sequence_information() = sequence_information;
      request_.set_force_confirm(force_confirm);

#if BUILDFLAG(IS_CHROMEOS_ASH)
      // Turn on/off the debug state flag (for Ash only).
      request_.set_health_data_logging_enabled(
          ::reporting::HistoryTracker::Get()->debug_state());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    }

    bool WriteRequest(dbus::MessageWriter* writer) override {
      return writer->AppendProtoAsArrayOfBytes(request_);
    }

    reporting::Status ParseResponse(dbus::MessageReader* reader) override {
      reporting::ConfirmRecordUploadResponse response_body;
      if (!reader->PopArrayOfBytesAsProto(&response_body)) {
        return reporting::Status(reporting::error::INTERNAL,
                                 "Response was not parsable.");
      }

#if BUILDFLAG(IS_CHROMEOS_ASH)
      // Accept health data if present (ChromeOS only)
      if (response_body.has_health_data()) {
        ::reporting::HistoryTracker::Get()->set_data(
            std::move(response_body.health_data()), base::DoNothing());
      }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

      reporting::Status status;
      status.RestoreFrom(response_body.status());
      return status;
    }

   private:
    reporting::ConfirmRecordUploadRequest request_;
  };

  void OwnerChanged(const std::string& old_owner,
                    const std::string& new_owner) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(origin_checker_);
    ServiceAvailable(/*service_is_available=*/!new_owner.empty());
  }

  void ServiceAvailable(bool service_is_available) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(origin_checker_);
    client_.SetAvailability(/*is_available=*/service_is_available);
  }

  scoped_refptr<dbus::ObjectProxy> missive_service_proxy_;

  reporting::DisconnectableClient client_;

  // Weak pointer factory - must be last member of the class.
  base::WeakPtrFactory<MissiveClientImpl> weak_ptr_factory_{this};
};

}  // namespace

MissiveClient::MissiveClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

MissiveClient::~MissiveClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

bool MissiveClient::has_valid_api_key() const {
  return has_valid_api_key_;
}

scoped_refptr<base::SequencedTaskRunner> MissiveClient::origin_task_runner()
    const {
  return origin_task_runner_;
}

// static
void MissiveClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  (new MissiveClientImpl())->Init(bus);
}

// static
void MissiveClient::InitializeFake() {
  (new FakeMissiveClient())->Init();
}

// static
void MissiveClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
MissiveClient* MissiveClient::Get() {
  return g_instance;
}

}  // namespace chromeos
