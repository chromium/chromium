// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_stats_recorder_impl.h"


#include "base/containers/circular_deque.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/gcm_driver/crypto/gcm_decryption_result.h"
#include "components/gcm_driver/crypto/gcm_encryption_provider.h"
#include "google_apis/gcm/engine/mcs_client.h"
#include "google_apis/gcm/engine/registration_request.h"

namespace gcm {

const uint32_t MAX_LOGGED_ACTIVITY_COUNT = 100;

namespace {

// Insert an item to the front of deque while maintaining the size of the deque.
// Overflow item is discarded.
//
// DANGER: the returned pointer will not be valind if the queue is modified.
template <typename T>
T* InsertCircularBuffer(base::circular_deque<T>* q, const T& item) {
  DCHECK(q);
  if (q->size() > MAX_LOGGED_ACTIVITY_COUNT - 1) {
    q->pop_back();
  }
  q->push_front(item);
  return &q->front();
}

// Helper for getting string representation of the MessageSendStatus enum.
std::string GetMessageSendStatusString(
    gcm::MCSClient::MessageSendStatus status) {
  switch (status) {
    case gcm::MCSClient::QUEUED:
      return "QUEUED";
    case gcm::MCSClient::SENT:
      return "SENT";
    case gcm::MCSClient::QUEUE_SIZE_LIMIT_REACHED:
      return "QUEUE_SIZE_LIMIT_REACHED";
    case gcm::MCSClient::APP_QUEUE_SIZE_LIMIT_REACHED:
      return "APP_QUEUE_SIZE_LIMIT_REACHED";
    case gcm::MCSClient::MESSAGE_TOO_LARGE:
      return "MESSAGE_TOO_LARGE";
    case gcm::MCSClient::NO_CONNECTION_ON_ZERO_TTL:
      return "NO_CONNECTION_ON_ZERO_TTL";
    case gcm::MCSClient::TTL_EXCEEDED:
      return "TTL_EXCEEDED";
    case gcm::MCSClient::SEND_STATUS_COUNT:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return "UNKNOWN";
}

// Helper for getting string representation of the
// ConnectionFactory::ConnectionResetReason enum.
std::string GetConnectionResetReasonString(
    gcm::ConnectionFactory::ConnectionResetReason reason) {
  switch (reason) {
    case gcm::ConnectionFactory::LOGIN_FAILURE:
      return "LOGIN_FAILURE";
    case gcm::ConnectionFactory::CLOSE_COMMAND:
      return "CLOSE_COMMAND";
    case gcm::ConnectionFactory::HEARTBEAT_FAILURE:
      return "HEARTBEAT_FAILURE";
    case gcm::ConnectionFactory::SOCKET_FAILURE:
      return "SOCKET_FAILURE";
    case gcm::ConnectionFactory::NETWORK_CHANGE:
      return "NETWORK_CHANGE";
    case gcm::ConnectionFactory::NEW_HEARTBEAT_INTERVAL:
      return "NEW_HEARTBEAT_INTERVAL";
    case gcm::ConnectionFactory::CONNECTION_RESET_COUNT:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return "UNKNOWN_REASON";
}

// Helper for getting string representation of the RegistrationRequest::Status
// enum.
std::string GetRegistrationStatusString(
    gcm::RegistrationRequest::Status status) {
  switch (status) {
    case gcm::RegistrationRequest::SUCCESS:
      return "SUCCESS";
    case gcm::RegistrationRequest::INVALID_PARAMETERS:
      return "INVALID_PARAMETERS";
    case gcm::RegistrationRequest::INVALID_SENDER:
      return "INVALID_SENDER";
    case gcm::RegistrationRequest::AUTHENTICATION_FAILED:
      return "AUTHENTICATION_FAILED";
    case gcm::RegistrationRequest::DEVICE_REGISTRATION_ERROR:
      return "DEVICE_REGISTRATION_ERROR";
    case gcm::RegistrationRequest::UNKNOWN_ERROR:
      return "UNKNOWN_ERROR";
    case gcm::RegistrationRequest::URL_FETCHING_FAILED:
      return "URL_FETCHING_FAILED";
    case gcm::RegistrationRequest::HTTP_NOT_OK:
      return "HTTP_NOT_OK";
    case gcm::RegistrationRequest::NO_RESPONSE_BODY:
      return "NO_RESPONSE_BODY";
    case gcm::RegistrationRequest::REACHED_MAX_RETRIES:
      return "REACHED_MAX_RETRIES";
    case gcm::RegistrationRequest::RESPONSE_PARSING_FAILED:
      return "RESPONSE_PARSING_FAILED";
    case gcm::RegistrationRequest::INTERNAL_SERVER_ERROR:
      return "INTERNAL_SERVER_ERROR";
    case gcm::RegistrationRequest::QUOTA_EXCEEDED:
      return "QUOTA_EXCEEDED";
    case gcm::RegistrationRequest::TOO_MANY_REGISTRATIONS:
      return "TOO_MANY_REGISTRATIONS";
    case gcm::RegistrationRequest::TOO_MANY_SUBSCRIBERS:
      return "TOO_MANY_SUBSCRIBERS";
    case gcm::RegistrationRequest::FIS_AUTH_ERROR:
      return "FIS_AUTH_ERROR";
    case gcm::RegistrationRequest::INVALID_TARGET_VERSION:
      return "INVALID_TARGET_VERSION";
  }
  return "UNKNOWN_STATUS";
}

// Helper for getting string representation of the RegistrationRequest::Status
// enum.
std::string GetUnregistrationStatusString(
    gcm::UnregistrationRequest::Status status) {
  switch (status) {
    case gcm::UnregistrationRequest::SUCCESS:
      return "SUCCESS";
    case gcm::UnregistrationRequest::URL_FETCHING_FAILED:
      return "URL_FETCHING_FAILED";
    case gcm::UnregistrationRequest::NO_RESPONSE_BODY:
      return "NO_RESPONSE_BODY";
    case gcm::UnregistrationRequest::RESPONSE_PARSING_FAILED:
      return "RESPONSE_PARSING_FAILED";
    case gcm::UnregistrationRequest::INCORRECT_APP_ID:
      return "INCORRECT_APP_ID";
    case gcm::UnregistrationRequest::INVALID_PARAMETERS:
      return "INVALID_PARAMETERS";
    case gcm::UnregistrationRequest::SERVICE_UNAVAILABLE:
      return "SERVICE_UNAVAILABLE";
    case gcm::UnregistrationRequest::INTERNAL_SERVER_ERROR:
      return "INTERNAL_SERVER_ERROR";
    case gcm::UnregistrationRequest::HTTP_NOT_OK:
      return "HTTP_NOT_OK";
    case gcm::UnregistrationRequest::UNKNOWN_ERROR:
      return "UNKNOWN_ERROR";
    case gcm::UnregistrationRequest::REACHED_MAX_RETRIES:
      return "REACHED_MAX_RETRIES";
    case gcm::UnregistrationRequest::DEVICE_REGISTRATION_ERROR:
      return "DEVICE_REGISTRATION_ERROR";
    case gcm::UnregistrationRequest::UNREGISTRATION_STATUS_COUNT:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return "UNKNOWN_STATUS";
}

}  // namespace

GCMStatsRecorderImpl::GCMStatsRecorderImpl()
    : is_recording_(false), delegate_(nullptr) {}

GCMStatsRecorderImpl::~GCMStatsRecorderImpl() = default;

void GCMStatsRecorderImpl::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void GCMStatsRecorderImpl::Clear() {
  checkin_activities_.clear();
  connection_activities_.clear();
  registration_activities_.clear();
  receiving_activities_.clear();
  sending_activities_.clear();
  decryption_failure_activities_.clear();
}

void GCMStatsRecorderImpl::NotifyActivityRecorded() {
  if (delegate_)
    delegate_->OnActivityRecorded();
}

void GCMStatsRecorderImpl::RecordDecryptionFailure(const std::string& app_id,
                                                   GCMDecryptionResult result) {
  DCHECK_NE(result, GCMDecryptionResult::UNENCRYPTED);
  DCHECK_NE(result, GCMDecryptionResult::DECRYPTED_DRAFT_03);
  DCHECK_NE(result, GCMDecryptionResult::DECRYPTED_DRAFT_08);
  if (!is_recording_)
    return;

  DecryptionFailureActivity data;
  DecryptionFailureActivity* inserted_data = InsertCircularBuffer(
      &decryption_failure_activities_, data);
  inserted_data->app_id = app_id;
  inserted_data->details = ToGCMDecryptionResultDetailsString(result);

  NotifyActivityRecorded();
}

void GCMStatsRecorderImpl::RecordCheckin(
    const std::string& event,
    const std::string& details) {
  CheckinActivity data;
  CheckinActivity* inserted_data = InsertCircularBuffer(
      &checkin_activities_, data);
  inserted_data->event = event;
  inserted_data->details = details;
  NotifyActivityRecorded();
}

void GCMStatsRecorderImpl::RecordCheckinInitiated(uint64_t android_id) {
  if (!is_recording_)
    return;
  RecordCheckin("Checkin initiated",
                base::StringPrintf("Android Id: %" PRIu64, android_id));
}

void GCMStatsRecorderImpl::RecordCheckinDelayedDueToBackoff(
    int64_t delay_msec) {
  if (!is_recording_)
    return;
  RecordCheckin("Checkin backoff",
                base::StringPrintf("Delayed for %" PRId64 " msec",
                                   delay_msec));
}

void GCMStatsRecorderImpl::RecordCheckinSuccess() {
  if (!is_recording_)
    return;
  RecordCheckin("Checkin succeeded", std::string());
}

void GCMStatsRecorderImpl::RecordCheckinFailure(const std::string& status,
                                                bool will_retry) {
  if (!is_recording_)
    return;
  RecordCheckin("Checkin failed", base::StringPrintf(
      "%s.%s",
      status.c_str(),
      will_retry ? " Will retry." : "Will not retry."));
}

void GCMStatsRecorderImpl::RecordConnection(
    const std::string& event,
    const std::string& details) {
  ConnectionActivity data;
  ConnectionActivity* inserted_data = InsertCircularBuffer(
      &connection_activities_, data);
  inserted_data->event = event;
  inserted_data->details = details;
  NotifyActivityRecorded();
}

void GCMStatsRecorderImpl::RecordConnectionInitiated(const std::string& host) {
  if (!is_recording_)
    return;

  RecordConnection("Connection initiated", host);
}

void GCMStatsRecorderImpl::RecordConnectionDelayedDueToBackoff(
    int64_t delay_msec) {
  if (!is_recording_)
    return;

  RecordConnection("Connection backoff",
                   base::StringPrintf("Delayed for %" PRId64 " msec",
                                      delay_msec));
}

void GCMStatsRecorderImpl::RecordConnectionSuccess() {
  if (!is_recording_)
    return;
  RecordConnection("Connection succeeded", std::string());
}

void GCMStatsRecorderImpl::RecordConnectionFailure(int network_error) {
  if (!is_recording_)
    return;
  RecordConnection("Connection failed",
                   base::StringPrintf("With network error %d", network_error));
}

void GCMStatsRecorderImpl::RecordConnectionResetSignaled(
      ConnectionFactory::ConnectionResetReason reason) {
  if (!is_recording_)
    return;
  RecordConnection("Connection reset",
                   GetConnectionResetReasonString(reason));
}

void GCMStatsRecorderImpl::RecordRegistration(
    const std::string& app_id,
    const std::string& source,
    const std::string& event,
    const std::string& details) {
  RegistrationActivity data;
  RegistrationActivity* inserted_data = InsertCircularBuffer(
      &registration_activities_, data);
  inserted_data->app_id = app_id;
  inserted_data->source = source;
  inserted_data->event = event;
  inserted_data->details = details;
  NotifyActivityRecorded();
}

void GCMStatsRecorderImpl::RecordRegistrationSent(
    const std::string& app_id,
    const std::string& sender_ids) {
  UMA_HISTOGRAM_COUNTS_1M("GCM.RegistrationRequest", 1);
  if (!is_recording_)
    return;
  RecordRegistration(app_id, sender_ids,
                     "Registration request sent", std::string());
}

void GCMStatsRecorderImpl::RecordRegistrationResponse(
    const std::string& app_id,
    const std::string& source,
    RegistrationRequest::Status status) {
  if (!is_recording_)
    return;
  RecordRegistration(app_id, source,
                     "Registration response received",
                     GetRegistrationStatusString(status));
}

void GCMStatsRecorderImpl::RecordRegistrationRetryDelayed(
    const std::string& app_id,
    const std::string& source,
    int64_t delay_msec,
    int retries_left) {
  if (!is_recording_)
    return;
  RecordRegistration(
      app_id,
      source,
      "Registration retry delayed",
      base::StringPrintf("Delayed for %" PRId64 " msec, retries left: %d",
                         delay_msec,
                         retries_left));
}

void GCMStatsRecorderImpl::RecordUnregistrationSent(const std::string& app_id,
                                                    const std::string& source) {
  if (!is_recording_)
    return;
  RecordRegistration(app_id, source, "Unregistration request sent",
                     std::string());
}

void GCMStatsRecorderImpl::RecordUnregistrationResponse(
    const std::string& app_id,
    const std::string& source,
    UnregistrationRequest::Status status) {
  if (!is_recording_)
    return;
  RecordRegistration(app_id,
                     source,
                     "Unregistration response received",
                     GetUnregistrationStatusString(status));
}

void GCMStatsRecorderImpl::RecordUnregistrationRetryDelayed(
    const std::string& app_id,
    const std::string& source,
    int64_t delay_msec,
    int retries_left) {
  if (!is_recording_)
    return;
  RecordRegistration(
      app_id,
      source,
      "Unregistration retry delayed",
      base::StringPrintf("Delayed for %" PRId64 " msec, retries left: %d",
                         delay_msec,
                         retries_left));
}

void GCMStatsRecorderImpl::RecordReceiving(
    const std::string& app_id,
    const std::string& from,
    int message_byte_size,
    const std::string& event,
    const std::string& details) {
  ReceivingActivity data;
  ReceivingActivity* inserted_data = InsertCircularBuffer(
      &receiving_activities_, data);
  inserted_data->app_id = app_id;
  inserted_data->from = from;
  inserted_data->message_byte_size = message_byte_size;
  inserted_data->event = event;
  inserted_data->details = details;
  NotifyActivityRecorded();
}

void GCMStatsRecorderImpl::RecordDataMessageReceived(
    const std::string& app_id,
    const std::string& from,
    int message_byte_size,
    ReceivedMessageType message_type) {
  if (!is_recording_)
    return;

  switch (message_type) {
    case GCMStatsRecorderImpl::DATA_MESSAGE:
      RecordReceiving(app_id, from, message_byte_size, "Data msg received",
                      std::string());
      break;
    case GCMStatsRecorderImpl::DELETED_MESSAGES:
      RecordReceiving(app_id, from, message_byte_size, "Data msg received",
                      "Message has been deleted on server");
      break;
  }
}

void GCMStatsRecorderImpl::CollectActivities(
    RecordedActivities* recorded_activities) const {
  recorded_activities->checkin_activities.insert(
      recorded_activities->checkin_activities.begin(),
      checkin_activities_.begin(),
      checkin_activities_.end());
  recorded_activities->connection_activities.insert(
      recorded_activities->connection_activities.begin(),
      connection_activities_.begin(),
      connection_activities_.end());
  recorded_activities->registration_activities.insert(
      recorded_activities->registration_activities.begin(),
      registration_activities_.begin(),
      registration_activities_.end());
  recorded_activities->receiving_activities.insert(
      recorded_activities->receiving_activities.begin(),
      receiving_activities_.begin(),
      receiving_activities_.end());
  recorded_activities->sending_activities.insert(
      recorded_activities->sending_activities.begin(),
      sending_activities_.begin(),
      sending_activities_.end());
  recorded_activities->decryption_failure_activities.insert(
      recorded_activities->decryption_failure_activities.begin(),
      decryption_failure_activities_.begin(),
      decryption_failure_activities_.end());
}

void GCMStatsRecorderImpl::RecordSending(const std::string& app_id,
                                         const std::string& receiver_id,
                                         const std::string& message_id,
                                         const std::string& event,
                                         const std::string& details) {
  SendingActivity data;
  SendingActivity* inserted_data = InsertCircularBuffer(
      &sending_activities_, data);
  inserted_data->app_id = app_id;
  inserted_data->receiver_id = receiver_id;
  inserted_data->message_id = message_id;
  inserted_data->event = event;
  inserted_data->details = details;
  NotifyActivityRecorded();
}

void GCMStatsRecorderImpl::RecordDataSentToWire(
    const std::string& app_id,
    const std::string& receiver_id,
    const std::string& message_id,
    int queued) {
  if (!is_recording_)
    return;
  RecordSending(app_id, receiver_id, message_id, "Data msg sent to wire",
                base::StringPrintf("Msg queued for %d seconds", queued));
}

void GCMStatsRecorderImpl::RecordNotifySendStatus(
    const std::string& app_id,
    const std::string& receiver_id,
    const std::string& message_id,
    gcm::MCSClient::MessageSendStatus status,
    int byte_size,
    int ttl) {
  if (!is_recording_)
    return;
  RecordSending(
      app_id,
      receiver_id,
      message_id,
      base::StringPrintf("SEND status: %s",
                         GetMessageSendStatusString(status).c_str()),
      base::StringPrintf("Msg size: %d bytes, TTL: %d", byte_size, ttl));
}

void GCMStatsRecorderImpl::RecordIncomingSendError(
    const std::string& app_id,
    const std::string& receiver_id,
    const std::string& message_id) {
  if (!is_recording_)
    return;
  RecordSending(app_id, receiver_id, message_id, "Received 'send error' msg",
                std::string());
}

}  // namespace gcm
