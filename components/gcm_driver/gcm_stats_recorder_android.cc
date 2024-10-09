// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "components/gcm_driver/crypto/gcm_decryption_result.h"
#include "components/gcm_driver/gcm_stats_recorder_android.h"

namespace gcm {

namespace {

const size_t MAX_LOGGED_ACTIVITY_COUNT = 100;

const char kSuccess[] = "SUCCESS";
const char kUnknownError[] = "UNKNOWN_ERROR";

}  // namespace

GCMStatsRecorderAndroid::GCMStatsRecorderAndroid(Delegate* delegate)
    : delegate_(delegate) {}

GCMStatsRecorderAndroid::~GCMStatsRecorderAndroid() = default;

void GCMStatsRecorderAndroid::Clear() {
  registration_activities_.clear();
  receiving_activities_.clear();
  decryption_failure_activities_.clear();
}

void GCMStatsRecorderAndroid::CollectActivities(
    RecordedActivities* recorded_activities) const {
  DCHECK(recorded_activities);

  recorded_activities->registration_activities.insert(
      recorded_activities->registration_activities.begin(),
      registration_activities_.begin(),
      registration_activities_.end());
  recorded_activities->receiving_activities.insert(
      recorded_activities->receiving_activities.begin(),
      receiving_activities_.begin(),
      receiving_activities_.end());
  recorded_activities->decryption_failure_activities.insert(
      recorded_activities->decryption_failure_activities.begin(),
      decryption_failure_activities_.begin(),
      decryption_failure_activities_.end());
}

void GCMStatsRecorderAndroid::RecordRegistrationSent(
    const std::string& app_id) {
  if (!is_recording_)
    return;

  RecordRegistration(app_id, "Registration request sent", "" /* details */);
}

void GCMStatsRecorderAndroid::RecordRegistrationResponse(
    const std::string& app_id,
    bool success) {
  if (!is_recording_)
    return;

  RecordRegistration(
      app_id, "Registration response received", success ? kSuccess
                                                        : kUnknownError);
}

void GCMStatsRecorderAndroid::RecordUnregistrationSent(
    const std::string& app_id) {
  if (!is_recording_)
    return;

  RecordRegistration(app_id, "Unregistration request sent", "" /* details */);
}

void GCMStatsRecorderAndroid::RecordUnregistrationResponse(
    const std::string& app_id,
    bool success) {
  if (!is_recording_)
    return;

  RecordRegistration(
      app_id, "Unregistration response received", success ? kSuccess
                                                          : kUnknownError);
}

void GCMStatsRecorderAndroid::RecordRegistration(const std::string& app_id,
                                                 const std::string& event,
                                                 const std::string& details) {
  RegistrationActivity activity;
  activity.app_id = app_id;
  activity.event = event;
  activity.details = details;

  // TODO(peter): Include the |source| (sender id(s)) of the registrations.

  registration_activities_.push_front(activity);
  if (registration_activities_.size() > MAX_LOGGED_ACTIVITY_COUNT)
    registration_activities_.pop_back();

  if (delegate_)
    delegate_->OnActivityRecorded();
}

void GCMStatsRecorderAndroid::RecordDataMessageReceived(
    const std::string& app_id,
    const std::string& from,
    int message_byte_size) {
  if (!is_recording_)
    return;

  ReceivingActivity activity;
  activity.app_id = app_id;
  activity.from = from;
  activity.message_byte_size = message_byte_size;
  activity.event = "Data msg received";

  receiving_activities_.push_front(activity);
  if (receiving_activities_.size() > MAX_LOGGED_ACTIVITY_COUNT)
    receiving_activities_.pop_back();

  if (delegate_)
    delegate_->OnActivityRecorded();
}

void GCMStatsRecorderAndroid::RecordDecryptionFailure(
    const std::string& app_id,
    GCMDecryptionResult result) {
  DCHECK_NE(result, GCMDecryptionResult::UNENCRYPTED);
  DCHECK_NE(result, GCMDecryptionResult::DECRYPTED_DRAFT_03);
  DCHECK_NE(result, GCMDecryptionResult::DECRYPTED_DRAFT_08);
  if (!is_recording_)
    return;

  DecryptionFailureActivity activity;
  activity.app_id = app_id;
  activity.details = ToGCMDecryptionResultDetailsString(result);

  decryption_failure_activities_.push_front(activity);
  if (decryption_failure_activities_.size() > MAX_LOGGED_ACTIVITY_COUNT)
    decryption_failure_activities_.pop_back();

  if (delegate_)
    delegate_->OnActivityRecorded();
}

}  // namespace gcm
