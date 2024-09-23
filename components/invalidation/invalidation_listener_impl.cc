// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/invalidation_listener_impl.h"

#include "base/containers/map_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_util.h"

namespace invalidation {

namespace {
const char kTypeKey[] = "type";
const char kPayloadKey[] = "payload";
const char kIssueTimestampMsKey[] = "issue_timestamp_ms";

constexpr char RegistrationMetricName[] =
    "FCMInvalidations.DirectInvalidation.RegistrationTokenRetrievalStatus";

// After the first failure, retry after 1 minute, then after 2, 4 etc up to a
// maximum of 1 day.
static constexpr net::BackoffEntry::Policy kRegistrationRetryBackoffPolicy = {
    .num_errors_to_ignore = 0,
    .initial_delay_ms = base::Minutes(1).InMilliseconds(),
    .multiply_factor = 2,
    .jitter_factor = 0.1,
    .maximum_backoff_ms = base::Days(1).InMilliseconds(),
    .always_use_initial_delay = true,
};

std::string GetValueFromMessage(const gcm::IncomingMessage& message,
                                const std::string& key) {
  const gcm::MessageData::const_iterator it = message.data.find(key);
  if (it != message.data.end()) {
    return it->second;
  }
  return std::string();
}

DirectInvalidation ParseIncomingMessage(const gcm::IncomingMessage& message) {
  const std::string type = GetValueFromMessage(message, kTypeKey);
  const std::string issue_timestamp_ms_str =
      GetValueFromMessage(message, kIssueTimestampMsKey);
  int64_t issue_timestamp_ms = 0;
  if (!base::StringToInt64(issue_timestamp_ms_str, &issue_timestamp_ms)) {
    issue_timestamp_ms = 0;
  }

  // The legacy invalidation version is the timestamp in microseconds.
  // TODO(b/341376574): Replace the version` with issue timestamp once we
  // fully migrate to direct message invalidations.
  const int64_t version =
      base::Milliseconds(issue_timestamp_ms).InMicroseconds();

  const std::string payload = GetValueFromMessage(message, kPayloadKey);
  return DirectInvalidation(type, version, payload);
}

// Insert or update the invalidation in the map at `invalidation.type()`.
// If `map` does not have an invalidation for that type, a copy of
// `invalidation` will be inserted.
// Otherwise, the existing invalidation for the type will be replaced by
// `invalidation` if and only if `invalidation` has a higher version than
// `map.at(invalidation.type())`.
void Upsert(std::map<Topic, DirectInvalidation>& map,
            const DirectInvalidation& invalidation) {
  const auto it = map.find(invalidation.type());
  if (it == map.end()) {
    map.emplace(invalidation.type(), invalidation);
    return;
  }
  if (it->second.version() < invalidation.version()) {
    it->second = invalidation;
    return;
  }
}

}  // namespace

InvalidationListenerImpl::InvalidationListenerImpl(
    gcm::GCMDriver* gcm_driver,
    instance_id::InstanceIDDriver* instance_id_driver,
    std::string project_number,
    std::string log_prefix)
    : gcm_driver_(gcm_driver),
      instance_id_driver_(instance_id_driver),
      project_number_(std::move(project_number)),
      log_prefix_(log_prefix),
      registration_retry_backoff_(&kRegistrationRetryBackoffPolicy) {
  LOG(WARNING) << log_prefix_
               << " Created for project_number: " << project_number_;
}

InvalidationListenerImpl::~InvalidationListenerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(WARNING) << log_prefix_ << " Destroying";
}

// InvalidationListener overrides.
void InvalidationListenerImpl::AddObserver(Observer* observer) {
  const std::string type = observer->GetType();
  CHECK(!type_to_handler_.contains(type));
  CHECK(!observers_.HasObserver(observer));

  LOG(WARNING) << log_prefix_ << " Observed by " << observer->GetType();

  observers_.AddObserver(observer);
  type_to_handler_[type] = observer;
  observer->OnExpectationChanged(AreInvalidationsExpected());

  const auto cache_map_node = type_to_invalidation_cache_.extract(type);
  if (!cache_map_node.empty()) {
    observer->OnInvalidationReceived(cache_map_node.mapped());
  }
}

bool InvalidationListenerImpl::HasObserver(const Observer* observer) const {
  return observers_.HasObserver(observer);
}

void InvalidationListenerImpl::RemoveObserver(const Observer* observer) {
  const std::string& type = observer->GetType();
  CHECK(type_to_handler_.contains(type));
  type_to_handler_.erase(type);
  observers_.RemoveObserver(observer);

  LOG(WARNING) << log_prefix_ << " Stopped observation by "
               << observer->GetType();
}

void InvalidationListenerImpl::Start(
    RegistrationTokenHandler* registration_token_handler) {
  // Does not allow double start.
  CHECK(!registration_token_handler_);

  LOG(WARNING) << log_prefix_ << " Starting";

  // Note that `AddAppHandler()` causes an immediate replay of all received
  // invalidations in background on Android.
  gcm_driver_->AddAppHandler(kFmAppId, this);

  registration_token_handler_ = registration_token_handler;
  FetchRegistrationToken();
}

void InvalidationListenerImpl::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(observers_.empty());
  CHECK(type_to_handler_.empty());

  registration_token_handler_ = nullptr;
  gcm_driver_->RemoveAppHandler(kFmAppId);
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void InvalidationListenerImpl::SetRegistrationUploadStatus(
    RegistrationTokenUploadStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  registration_upload_status_ = status;
  UpdateObserversExpectations();
}

// GCMAppHandler overrides.
void InvalidationListenerImpl::ShutdownHandler() {
  NOTREACHED()
      << "Shutdown() should come before and it removes us from the list of app "
         "handlers of gcm::GCMDriver so this shouldn't ever been called.";
}

void InvalidationListenerImpl::OnStoreReset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The FCM registration token is not stored by FCMHandler.
}

void InvalidationListenerImpl::OnMessage(const std::string& app_id,
                                         const gcm::IncomingMessage& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(app_id, kFmAppId);

  LOG(WARNING) << log_prefix_ << " Message received";
  for (const auto& [key, value] : message.data) {
    LOG(WARNING) << log_prefix_ << " " << key << "->" << value;
  }

  const DirectInvalidation invalidation = ParseIncomingMessage(message);

  Observer* observer =
      base::FindPtrOrNull(type_to_handler_, invalidation.type());
  if (observer) {
    observer->OnInvalidationReceived(invalidation);
    return;
  }

  // Only cache when there is not an observer for this message `type` yet,
  // so that the listener can still deliver the message after the appropriate
  // observer is attached.
  // Otherwise, the listener directly passes the message to an observer
  // without caching.
  Upsert(type_to_invalidation_cache_, invalidation);
}

void InvalidationListenerImpl::OnMessagesDeleted(const std::string& app_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(app_id, kFmAppId);
}

void InvalidationListenerImpl::OnSendError(
    const std::string& app_id,
    const gcm::GCMClient::SendErrorDetails& details) {
  NOTREACHED() << "Should never be called because the invalidation "
                  "service doesn't send GCM messages to the server.";
}

void InvalidationListenerImpl::OnSendAcknowledged(
    const std::string& app_id,
    const std::string& message_id) {
  NOTREACHED() << "Should never be called because the invalidation "
                  "service doesn't send GCM messages to the server.";
}

void InvalidationListenerImpl::FetchRegistrationToken() {
  instance_id_driver_->GetInstanceID(kFmAppId)->GetToken(
      project_number_, instance_id::kGCMScope,
      /*time_to_live=*/kRegistrationTokenTimeToLive,
      /*flags=*/{instance_id::InstanceID::Flags::kIsLazy},
      base::BindOnce(&InvalidationListenerImpl::OnRegistrationTokenReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void InvalidationListenerImpl::OnRegistrationTokenReceived(
    const std::string& new_registration_token,
    instance_id::InstanceID::Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(registration_token_handler_);

  base::UmaHistogramEnumeration(RegistrationMetricName, result);

  const bool succeeded = result == instance_id::InstanceID::SUCCESS;

  registration_retry_backoff_.InformOfRequest(succeeded);

  if (succeeded) {
    registration_token_ = new_registration_token;
    LOG(WARNING) << log_prefix_
                 << " Registration token: " << new_registration_token;
    registration_token_handler_->OnRegistrationTokenReceived(
        registration_token_.value(),
        base::Time::Now() + kRegistrationTokenTimeToLive);
    registration_retry_backoff_.Reset();
  } else {
    LOG(WARNING) << log_prefix_ << " Message subscription failed: " << result;
    registration_token_ = std::nullopt;
  }

  UpdateObserversExpectations();

  // Schedule the next registration token refresh or retry attempt.
  const base::TimeDelta delay =
      succeeded ? kRegistrationTokenValidationPeriod
                : registration_retry_backoff_.GetTimeUntilRelease();
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&InvalidationListenerImpl::FetchRegistrationToken,
                     weak_ptr_factory_.GetWeakPtr()),
      delay);
}

InvalidationsExpected InvalidationListenerImpl::AreInvalidationsExpected()
    const {
  return (registration_token_ && registration_upload_status_ ==
                                     RegistrationTokenUploadStatus::kSucceeded)
             ? InvalidationsExpected::kYes
             : InvalidationsExpected::kMaybe;
}

void InvalidationListenerImpl::UpdateObserversExpectations() {
  const InvalidationsExpected expected = AreInvalidationsExpected();
  for (auto& observer : observers_) {
    observer.OnExpectationChanged(expected);
  }
}

}  // namespace invalidation
