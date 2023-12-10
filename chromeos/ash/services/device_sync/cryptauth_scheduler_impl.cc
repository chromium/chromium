// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_scheduler_impl.h"

#include <algorithm>
#include <utility>

#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/services/device_sync/pref_names.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_logging.h"
#include "chromeos/ash/services/device_sync/value_string_encoding.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash::device_sync {

namespace {

constexpr base::TimeDelta kZeroTimeDelta = base::Seconds(0);

// The default period between successful enrollments in days. Superseded by the
// ClientDirective's checkin_delay_millis sent by CryptAuth.
constexpr base::TimeDelta kDefaultRefreshPeriod = base::Days(30);

// The default period, in hours, between Enrollment/DeviceSync attempts if the
// previous Enrollment/DeviceSync attempt failed. Superseded by the
// ClientDirective's retry_period_millis sent by CryptAuth.
constexpr base::TimeDelta kDefaultRetryPeriod = base::Hours(12);

// The time to wait before an "immediate" retry attempt after a failed
// Enrollment/DeviceSync attempt. Note: Some request types are throttled by
// CryptAuth if more than one is sent within a five-minute window.
constexpr base::TimeDelta kImmediateRetryDelay = base::Minutes(5);

// The default number of "immediate" retries after a failed
// Enrollment/DeviceSync attempt. Superseded by the ClientDirective's
// retry_attempts sent by CryptAuth.
const int kDefaultMaxImmediateRetries = 3;

const char kNoClientDirective[] = "[No ClientDirective]";

const char kNoClientMetadata[] = "[No ClientMetadata]";

bool IsClientDirectiveValid(
    const cryptauthv2::ClientDirective& client_directive) {
  return client_directive.checkin_delay_millis() > 0 &&
         client_directive.retry_period_millis() > 0 &&
         client_directive.retry_attempts() >= 0;
}

// Fills a ClientDirective with our chosen default parameters. This
// ClientDirective is used until a ClientDirective is received from CryptAuth.
cryptauthv2::ClientDirective CreateDefaultClientDirective() {
  cryptauthv2::ClientDirective client_directive;
  client_directive.set_checkin_delay_millis(
      kDefaultRefreshPeriod.InMilliseconds());
  client_directive.set_retry_period_millis(
      kDefaultRetryPeriod.InMilliseconds());
  client_directive.set_retry_attempts(kDefaultMaxImmediateRetries);

  return client_directive;
}

cryptauthv2::ClientDirective BuildClientDirective(PrefService* pref_service) {
  DCHECK(pref_service);
  const base::Value& encoded_client_directive =
      pref_service->GetValue(prefs::kCryptAuthSchedulerClientDirective);
  if (encoded_client_directive.GetString() == kNoClientDirective)
    return CreateDefaultClientDirective();

  std::optional<cryptauthv2::ClientDirective> client_directive_from_pref =
      util::DecodeProtoMessageFromValueString<cryptauthv2::ClientDirective>(
          &encoded_client_directive);

  return client_directive_from_pref.value_or(CreateDefaultClientDirective());
}

cryptauthv2::ClientMetadata BuildClientMetadata(
    size_t retry_count,
    const cryptauthv2::ClientMetadata::InvocationReason& invocation_reason,
    const std::optional<std::string>& session_id) {
  cryptauthv2::ClientMetadata client_metadata;
  client_metadata.set_retry_count(retry_count);
  client_metadata.set_invocation_reason(invocation_reason);
  if (session_id)
    client_metadata.set_session_id(*session_id);

  return client_metadata;
}

}  // namespace

// static
CryptAuthSchedulerImpl::Factory*
    CryptAuthSchedulerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<CryptAuthScheduler> CryptAuthSchedulerImpl::Factory::Create(
    PrefService* pref_service,
    NetworkStateHandler* network_state_handler,
    base::Clock* clock,
    std::unique_ptr<base::OneShotTimer> enrollment_timer,
    std::unique_ptr<base::OneShotTimer> device_sync_timer) {
  if (test_factory_) {
    return test_factory_->CreateInstance(pref_service, network_state_handler,
                                         clock, std::move(enrollment_timer),
                                         std::move(device_sync_timer));
  }

  return base::WrapUnique(new CryptAuthSchedulerImpl(
      pref_service, network_state_handler, clock, std::move(enrollment_timer),
      std::move(device_sync_timer)));
}

// static
void CryptAuthSchedulerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

CryptAuthSchedulerImpl::Factory::~Factory() = default;

// static
void CryptAuthSchedulerImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kCryptAuthSchedulerClientDirective,
                               kNoClientDirective);
  registry->RegisterStringPref(
      prefs::kCryptAuthSchedulerNextEnrollmentRequestClientMetadata,
      kNoClientMetadata);
  registry->RegisterStringPref(
      prefs::kCryptAuthSchedulerNextDeviceSyncRequestClientMetadata,
      kNoClientMetadata);
  registry->RegisterTimePref(
      prefs::kCryptAuthSchedulerLastEnrollmentAttemptTime, base::Time());
  registry->RegisterTimePref(
      prefs::kCryptAuthSchedulerLastDeviceSyncAttemptTime, base::Time());
  registry->RegisterTimePref(
      prefs::kCryptAuthSchedulerLastSuccessfulEnrollmentTime, base::Time());
  registry->RegisterTimePref(
      prefs::kCryptAuthSchedulerLastSuccessfulDeviceSyncTime, base::Time());
}

CryptAuthSchedulerImpl::CryptAuthSchedulerImpl(
    PrefService* pref_service,
    NetworkStateHandler* network_state_handler,
    base::Clock* clock,
    std::unique_ptr<base::OneShotTimer> enrollment_timer,
    std::unique_ptr<base::OneShotTimer> device_sync_timer)
    : pref_service_(pref_service),
      network_state_handler_(network_state_handler),
      clock_(clock),
      client_directive_(BuildClientDirective(pref_service)) {
  DCHECK(pref_service_);
  DCHECK(network_state_handler_);
  DCHECK(clock_);
  DCHECK(IsClientDirectiveValid(client_directive_));

  request_timers_[RequestType::kEnrollment] = std::move(enrollment_timer);
  request_timers_[RequestType::kDeviceSync] = std::move(device_sync_timer);

  // Queue up the most recently scheduled requests if applicable.
  InitializePendingRequest(RequestType::kEnrollment);
  InitializePendingRequest(RequestType::kDeviceSync);
}

CryptAuthSchedulerImpl::~CryptAuthSchedulerImpl() = default;

// static
std::string CryptAuthSchedulerImpl::GetLastAttemptTimePrefName(
    RequestType request_type) {
  switch (request_type) {
    case RequestType::kEnrollment:
      return prefs::kCryptAuthSchedulerLastEnrollmentAttemptTime;
    case RequestType::kDeviceSync:
      return prefs::kCryptAuthSchedulerLastDeviceSyncAttemptTime;
  }
}

// static
std::string CryptAuthSchedulerImpl::GetLastSuccessTimePrefName(
    RequestType request_type) {
  switch (request_type) {
    case RequestType::kEnrollment:
      return prefs::kCryptAuthSchedulerLastSuccessfulEnrollmentTime;
    case RequestType::kDeviceSync:
      return prefs::kCryptAuthSchedulerLastSuccessfulDeviceSyncTime;
  }
}

// static
std::string CryptAuthSchedulerImpl::GetPendingRequestPrefName(
    RequestType request_type) {
  switch (request_type) {
    case RequestType::kEnrollment:
      return prefs::kCryptAuthSchedulerNextEnrollmentRequestClientMetadata;
    case RequestType::kDeviceSync:
      return prefs::kCryptAuthSchedulerNextDeviceSyncRequestClientMetadata;
  }
}

void CryptAuthSchedulerImpl::OnEnrollmentSchedulingStarted() {
  OnSchedulingStarted(RequestType::kEnrollment);
}

void CryptAuthSchedulerImpl::OnDeviceSyncSchedulingStarted() {
  OnSchedulingStarted(RequestType::kDeviceSync);
}

void CryptAuthSchedulerImpl::RequestEnrollment(
    const cryptauthv2::ClientMetadata::InvocationReason& invocation_reason,
    const std::optional<std::string>& session_id) {
  MakeRequest(RequestType::kEnrollment, invocation_reason, session_id);
}

void CryptAuthSchedulerImpl::RequestDeviceSync(
    const cryptauthv2::ClientMetadata::InvocationReason& invocation_reason,
    const std::optional<std::string>& session_id) {
  MakeRequest(RequestType::kDeviceSync, invocation_reason, session_id);
}

void CryptAuthSchedulerImpl::HandleEnrollmentResult(
    const CryptAuthEnrollmentResult& enrollment_result) {
  HandleResult(RequestType::kEnrollment, enrollment_result.IsSuccess(),
               enrollment_result.client_directive());
}

void CryptAuthSchedulerImpl::HandleDeviceSyncResult(
    const CryptAuthDeviceSyncResult& device_sync_result) {
  // Note: "Success" for DeviceSync means no errors, not even non-fatal errors.
  HandleResult(RequestType::kDeviceSync, device_sync_result.IsSuccess(),
               device_sync_result.client_directive());
}

std::optional<base::Time>
CryptAuthSchedulerImpl::GetLastSuccessfulEnrollmentTime() const {
  return GetLastSuccessTime(RequestType::kEnrollment);
}

std::optional<base::Time>
CryptAuthSchedulerImpl::GetLastSuccessfulDeviceSyncTime() const {
  return GetLastSuccessTime(RequestType::kDeviceSync);
}

base::TimeDelta CryptAuthSchedulerImpl::GetRefreshPeriod() const {
  return base::Milliseconds(client_directive_.checkin_delay_millis());
}
std::optional<base::TimeDelta>
CryptAuthSchedulerImpl::GetTimeToNextEnrollmentRequest() const {
  return GetTimeToNextRequest(RequestType::kEnrollment);
}

std::optional<base::TimeDelta>
CryptAuthSchedulerImpl::GetTimeToNextDeviceSyncRequest() const {
  return GetTimeToNextRequest(RequestType::kDeviceSync);
}

bool CryptAuthSchedulerImpl::IsWaitingForEnrollmentResult() const {
  return IsWaitingForResult(RequestType::kEnrollment);
}

bool CryptAuthSchedulerImpl::IsWaitingForDeviceSyncResult() const {
  return IsWaitingForResult(RequestType::kDeviceSync);
}

size_t CryptAuthSchedulerImpl::GetNumConsecutiveEnrollmentFailures() const {
  return GetNumConsecutiveFailures(RequestType::kEnrollment);
}

size_t CryptAuthSchedulerImpl::GetNumConsecutiveDeviceSyncFailures() const {
  return GetNumConsecutiveFailures(RequestType::kDeviceSync);
}

void CryptAuthSchedulerImpl::DefaultNetworkChanged(
    const NetworkState* network) {
  // The updated default network may not be online.
  if (!DoesMachineHaveNetworkConnectivity())
    return;

  // Now that the device has connectivity, reschedule requests.
  ScheduleNextRequest(RequestType::kEnrollment);
  ScheduleNextRequest(RequestType::kDeviceSync);
}

void CryptAuthSchedulerImpl::OnShuttingDown() {
  DCHECK(network_state_handler_);
  network_state_handler_observer_.Reset();
  network_state_handler_ = nullptr;
}

void CryptAuthSchedulerImpl::OnSchedulingStarted(RequestType request_type) {
  if (!network_state_handler_observer_.IsObserving()) {
    DCHECK(network_state_handler_);
    network_state_handler_observer_.Observe(network_state_handler_.get());
  }

  ScheduleNextRequest(request_type);
}

void CryptAuthSchedulerImpl::MakeRequest(
    RequestType request_type,
    const cryptauthv2::ClientMetadata::InvocationReason& invocation_reason,
    const std::optional<std::string>& session_id) {
  request_timers_[request_type]->Stop();

  pending_requests_[request_type] =
      BuildClientMetadata(0 /* retry_count */, invocation_reason, session_id);

  ScheduleNextRequest(request_type);
}

void CryptAuthSchedulerImpl::HandleResult(
    RequestType request_type,
    bool success,
    const std::optional<cryptauthv2::ClientDirective>& new_client_directive) {
  DCHECK(current_requests_[request_type]);
  DCHECK(!request_timers_[request_type]->IsRunning());

  base::Time now = clock_->Now();

  pref_service_->SetTime(GetLastAttemptTimePrefName(request_type), now);

  if (new_client_directive && IsClientDirectiveValid(*new_client_directive)) {
    client_directive_ = *new_client_directive;
    PA_LOG(VERBOSE) << "New client directive:\n" << client_directive_;
    pref_service_->Set(
        prefs::kCryptAuthSchedulerClientDirective,
        util::EncodeProtoMessageAsValueString(&client_directive_));
  }

  // If successful, process InvokeNext field of ClientDirective. If unsuccessful
  // and a more immediate request isn't pending, queue up the failure recovery
  // attempt.
  if (success) {
    pref_service_->SetTime(GetLastSuccessTimePrefName(request_type), now);

    HandleInvokeNext(client_directive_.invoke_next(),
                     current_requests_[request_type]->session_id());
  } else if (!pending_requests_[request_type]) {
    current_requests_[request_type]->set_retry_count(
        current_requests_[request_type]->retry_count() + 1);
    pending_requests_[request_type] = current_requests_[request_type];
  }

  current_requests_[request_type].reset();

  // Because the ClientDirective might have changed, we update both timers.
  ScheduleNextRequest(RequestType::kEnrollment);
  ScheduleNextRequest(RequestType::kDeviceSync);
}

void CryptAuthSchedulerImpl::HandleInvokeNext(
    const ::google::protobuf::RepeatedPtrField<cryptauthv2::InvokeNext>&
        invoke_next_list,
    const std::string& session_id) {
  for (const cryptauthv2::InvokeNext& invoke_next : invoke_next_list) {
    if (invoke_next.service() == cryptauthv2::ENROLLMENT) {
      PA_LOG(VERBOSE) << "Enrollment requested by InvokeNext";
      RequestEnrollment(cryptauthv2::ClientMetadata::SERVER_INITIATED,
                        session_id);
    } else if (invoke_next.service() == cryptauthv2::DEVICE_SYNC) {
      PA_LOG(VERBOSE) << "DeviceSync requested by InvokeNext";
      RequestDeviceSync(cryptauthv2::ClientMetadata::SERVER_INITIATED,
                        session_id);
    } else {
      PA_LOG(WARNING) << "Unknown InvokeNext TargetService "
                      << invoke_next.service();
    }
  }
}

std::optional<base::Time> CryptAuthSchedulerImpl::GetLastSuccessTime(
    RequestType request_type) const {
  base::Time time =
      pref_service_->GetTime(GetLastSuccessTimePrefName(request_type));
  if (time.is_null())
    return std::nullopt;

  return time;
}

std::optional<base::TimeDelta> CryptAuthSchedulerImpl::GetTimeToNextRequest(
    RequestType request_type) const {
  // Request already in progress.
  if (IsWaitingForResult(request_type))
    return kZeroTimeDelta;

  // No pending request.
  const auto it = pending_requests_.find(request_type);
  if (it == pending_requests_.end() || !it->second)
    return std::nullopt;

  int64_t retry_count = it->second->retry_count();
  cryptauthv2::ClientMetadata::InvocationReason invocation_reason =
      it->second->invocation_reason();

  // If we are not recovering from failure, attempt all but periodic requests
  // immediately.
  if (retry_count == 0) {
    if (invocation_reason != cryptauthv2::ClientMetadata::PERIODIC)
      return kZeroTimeDelta;

    std::optional<base::Time> last_success_time =
        GetLastSuccessTime(request_type);
    DCHECK(last_success_time);

    base::TimeDelta time_since_last_success =
        clock_->Now() - *last_success_time;
    return std::max(kZeroTimeDelta,
                    GetRefreshPeriod() - time_since_last_success);
  }

  base::TimeDelta time_since_last_attempt =
      clock_->Now() -
      pref_service_->GetTime(GetLastAttemptTimePrefName(request_type));

  // Recover from failure using immediate retry.
  DCHECK(retry_count > 0);
  if (retry_count < client_directive_.retry_attempts()) {
    return std::max(kZeroTimeDelta,
                    kImmediateRetryDelay - time_since_last_attempt);
  }

  // Recover from failure after expending allotted immediate retries.
  return std::max(kZeroTimeDelta,
                  base::Milliseconds(client_directive_.retry_period_millis()) -
                      time_since_last_attempt);
}

bool CryptAuthSchedulerImpl::IsWaitingForResult(
    RequestType request_type) const {
  const auto it = current_requests_.find(request_type);
  return (it != current_requests_.end() && it->second);
}

size_t CryptAuthSchedulerImpl::GetNumConsecutiveFailures(
    RequestType request_type) const {
  const auto current_request_it = current_requests_.find(request_type);
  if (current_request_it != current_requests_.end() &&
      current_request_it->second) {
    return current_request_it->second->retry_count();
  }

  const auto pending_request_it = pending_requests_.find(request_type);
  if (pending_request_it != pending_requests_.end() &&
      pending_request_it->second) {
    return pending_request_it->second->retry_count();
  }

  return 0;
}

bool CryptAuthSchedulerImpl::DoesMachineHaveNetworkConnectivity() const {
  if (!network_state_handler_)
    return false;

  // TODO(khorimoto): IsConnectedState() can still return true if connected to
  // a captive portal; use the "online" boolean once we fetch data via the
  // networking Mojo API. See https://crbug.com/862420.
  const NetworkState* default_network =
      network_state_handler_->DefaultNetwork();
  return default_network && default_network->IsConnectedState();
}

void CryptAuthSchedulerImpl::InitializePendingRequest(
    RequestType request_type) {
  // Queue up the persisted scheduled request if applicable.
  const base::Value& client_metadata_from_pref =
      pref_service_->GetValue(GetPendingRequestPrefName(request_type));
  if (client_metadata_from_pref.GetString() != kNoClientMetadata) {
    pending_requests_[request_type] =
        util::DecodeProtoMessageFromValueString<cryptauthv2::ClientMetadata>(
            &client_metadata_from_pref);
  }

  // If we are recovering from a failure, reset the failure count to 1 in the
  // hopes that the restart solved the issue. This will allow for immediate
  // retries again if permitted by the ClientDirective.
  if (pending_requests_[request_type] &&
      pending_requests_[request_type]->retry_count() > 0) {
    pending_requests_[request_type]->set_retry_count(1);
  }
}

void CryptAuthSchedulerImpl::ScheduleNextRequest(RequestType request_type) {
  // Wait for the current attempt to finish before determining the next request
  // in case we need to recover from a failure.
  if (IsWaitingForResult(request_type))
    return;

  // For Enrollment only, if no request has already been explicitly made,
  // schedule a periodic attempt.
  if (request_type == RequestType::kEnrollment &&
      !pending_requests_[request_type]) {
    pending_requests_[request_type] =
        BuildClientMetadata(0 /* retry_count */,
                            GetLastSuccessTime(request_type)
                                ? cryptauthv2::ClientMetadata::PERIODIC
                                : cryptauthv2::ClientMetadata::INITIALIZATION,
                            std::nullopt /* session_id */);
  }

  // Schedule a first-time DeviceSync if one has never successfully completed.
  // However, unlike Enrollment, there are no periodic DeviceSyncs.
  if (request_type == RequestType::kDeviceSync &&
      !pending_requests_[request_type] && !GetLastSuccessTime(request_type)) {
    pending_requests_[request_type] = BuildClientMetadata(
        0 /* retry_count */, cryptauthv2::ClientMetadata::INITIALIZATION,
        std::nullopt /* session_id */);
  }

  if (!pending_requests_[request_type]) {
    // By this point, only DeviceSync can have no requests pending because it
    // does not schedule periodic syncs.
    DCHECK_EQ(RequestType::kDeviceSync, request_type);
    pref_service_->SetString(GetPendingRequestPrefName(request_type),
                             kNoClientMetadata);
    return;
  }

  // Persist the pending request even if scheduling hasn't started yet.
  pref_service_->Set(GetPendingRequestPrefName(request_type),
                     util::EncodeProtoMessageAsValueString(
                         &pending_requests_[request_type].value()));

  bool has_scheduling_started = (request_type == RequestType::kEnrollment &&
                                 HasEnrollmentSchedulingStarted()) ||
                                (request_type == RequestType::kDeviceSync &&
                                 HasDeviceSyncSchedulingStarted());
  if (!has_scheduling_started)
    return;

  std::optional<base::TimeDelta> delay = GetTimeToNextRequest(request_type);
  DCHECK(delay);
  request_timers_[request_type]->Start(
      FROM_HERE, *delay,
      base::BindOnce(&CryptAuthSchedulerImpl::OnTimerFired,
                     base::Unretained(this), request_type));
}

void CryptAuthSchedulerImpl::OnTimerFired(RequestType request_type) {
  DCHECK(!current_requests_[request_type]);
  DCHECK(pending_requests_[request_type]);

  if (!DoesMachineHaveNetworkConnectivity()) {
    std::string type_string =
        request_type == RequestType::kEnrollment ? "Enrollment" : "DeviceSync";
    PA_LOG(INFO) << type_string
                 << " triggered while the device is offline. Waiting "
                 << "for online connectivity before making request.";
    return;
  }

  current_requests_[request_type] = pending_requests_[request_type];
  pending_requests_[request_type].reset();

  switch (request_type) {
    case RequestType::kEnrollment: {
      std::optional<cryptauthv2::PolicyReference> policy_reference =
          std::nullopt;
      if (client_directive_.has_policy_reference())
        policy_reference = client_directive_.policy_reference();

      NotifyEnrollmentRequested(*current_requests_[request_type],
                                policy_reference);
      return;
    }
    case RequestType::kDeviceSync:
      NotifyDeviceSyncRequested(*current_requests_[request_type]);
      return;
  }
}

}  // namespace ash::device_sync
