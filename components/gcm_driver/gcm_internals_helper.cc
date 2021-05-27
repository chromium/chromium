// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_internals_helper.h"

#include <memory>
#include <utility>

#include "base/format_macros.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/gcm_driver/gcm_activity.h"
#include "components/gcm_driver/gcm_internals_constants.h"
#include "components/gcm_driver/gcm_profile_service.h"

namespace gcm_driver {

namespace {

void SetCheckinInfo(const std::vector<gcm::CheckinActivity>& checkins,
                    std::vector<base::Value>* checkin_info) {
  for (const gcm::CheckinActivity& checkin : checkins) {
    base::Value row(base::Value::Type::LIST);
    row.Append(checkin.time.ToJsTime());
    row.Append(checkin.event);
    row.Append(checkin.details);
    checkin_info->push_back(std::move(row));
  }
}

void SetConnectionInfo(const std::vector<gcm::ConnectionActivity>& connections,
                       std::vector<base::Value>* connection_info) {
  for (const gcm::ConnectionActivity& connection : connections) {
    base::Value row(base::Value::Type::LIST);
    row.Append(connection.time.ToJsTime());
    row.Append(connection.event);
    row.Append(connection.details);
    connection_info->push_back(std::move(row));
  }
}

void SetRegistrationInfo(
    const std::vector<gcm::RegistrationActivity>& registrations,
    std::vector<base::Value>* registration_info) {
  for (const gcm::RegistrationActivity& registration : registrations) {
    base::Value row(base::Value::Type::LIST);
    row.Append(registration.time.ToJsTime());
    row.Append(registration.app_id);
    row.Append(registration.source);
    row.Append(registration.event);
    row.Append(registration.details);
    registration_info->push_back(std::move(row));
  }
}

void SetReceivingInfo(const std::vector<gcm::ReceivingActivity>& receives,
                      std::vector<base::Value>* receive_info) {
  for (const gcm::ReceivingActivity& receive : receives) {
    base::Value row(base::Value::Type::LIST);
    row.Append(receive.time.ToJsTime());
    row.Append(receive.app_id);
    row.Append(receive.from);
    row.Append(base::NumberToString(receive.message_byte_size));
    row.Append(receive.event);
    row.Append(receive.details);
    receive_info->push_back(std::move(row));
  }
}

void SetSendingInfo(const std::vector<gcm::SendingActivity>& sends,
                    std::vector<base::Value>* send_info) {
  for (const gcm::SendingActivity& send : sends) {
    base::Value row(base::Value::Type::LIST);
    row.Append(send.time.ToJsTime());
    row.Append(send.app_id);
    row.Append(send.receiver_id);
    row.Append(send.message_id);
    row.Append(send.event);
    row.Append(send.details);
    send_info->push_back(std::move(row));
  }
}

void SetDecryptionFailureInfo(
    const std::vector<gcm::DecryptionFailureActivity>& failures,
    std::vector<base::Value>* failure_info) {
  for (const gcm::DecryptionFailureActivity& failure : failures) {
    base::Value row(base::Value::Type::LIST);
    row.Append(failure.time.ToJsTime());
    row.Append(failure.app_id);
    row.Append(failure.details);
    failure_info->push_back(std::move(row));
  }
}

}  // namespace

void SetGCMInternalsInfo(const gcm::GCMClient::GCMStatistics* stats,
                         gcm::GCMProfileService* profile_service,
                         PrefService* prefs,
                         base::DictionaryValue* results) {
  base::Value device_info(base::Value::Type::DICTIONARY);

  device_info.SetBoolKey(kProfileServiceCreated, profile_service != nullptr);
  device_info.SetBoolKey(kGcmEnabled, true);
  if (stats) {
    results->SetBoolKey(kIsRecording, stats->is_recording);
    device_info.SetBoolKey(kGcmClientCreated, stats->gcm_client_created);
    device_info.SetStringKey(kGcmClientState, stats->gcm_client_state);
    device_info.SetBoolKey(kConnectionClientCreated,
                           stats->connection_client_created);

    base::Value registered_app_ids(base::Value::Type::LIST);
    for (const std::string& app_id : stats->registered_app_ids)
      registered_app_ids.Append(app_id);

    device_info.SetKey(kRegisteredAppIds, std::move(registered_app_ids));

    if (stats->connection_client_created)
      device_info.SetStringKey(kConnectionState, stats->connection_state);
    if (!stats->last_checkin.is_null()) {
      device_info.SetStringKey(
          kLastCheckin, base::UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(
                            stats->last_checkin)));
    }
    if (!stats->next_checkin.is_null()) {
      device_info.SetStringKey(
          kNextCheckin, base::UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(
                            stats->next_checkin)));
    }
    if (stats->android_id > 0) {
      device_info.SetStringKey(
          kAndroidId, base::StringPrintf("0x%" PRIx64, stats->android_id));
    }
    if (stats->android_secret > 0) {
      device_info.SetStringKey(kAndroidSecret,
                               base::NumberToString(stats->android_secret));
    }
    device_info.SetIntKey(kSendQueueSize, stats->send_queue_size);
    device_info.SetIntKey(kResendQueueSize, stats->resend_queue_size);
    results->SetKey(kDeviceInfo, std::move(device_info));

    if (stats->recorded_activities.checkin_activities.size() > 0) {
      std::vector<base::Value> checkin_info;
      SetCheckinInfo(stats->recorded_activities.checkin_activities,
                     &checkin_info);
      results->SetKey(kCheckinInfo, base::Value(std::move(checkin_info)));
    }
    if (stats->recorded_activities.connection_activities.size() > 0) {
      std::vector<base::Value> connection_info;
      SetConnectionInfo(stats->recorded_activities.connection_activities,
                        &connection_info);
      results->SetKey(kConnectionInfo, base::Value(std::move(connection_info)));
    }
    if (stats->recorded_activities.registration_activities.size() > 0) {
      std::vector<base::Value> registration_info;
      SetRegistrationInfo(stats->recorded_activities.registration_activities,
                          &registration_info);
      results->SetKey(kRegistrationInfo,
                      base::Value(std::move(registration_info)));
    }
    if (stats->recorded_activities.receiving_activities.size() > 0) {
      std::vector<base::Value> receive_info;
      SetReceivingInfo(stats->recorded_activities.receiving_activities,
                       &receive_info);
      results->SetKey(kReceiveInfo, base::Value(std::move(receive_info)));
    }
    if (stats->recorded_activities.sending_activities.size() > 0) {
      std::vector<base::Value> send_info;
      SetSendingInfo(stats->recorded_activities.sending_activities, &send_info);
      results->SetKey(kSendInfo, base::Value(std::move(send_info)));
    }

    if (stats->recorded_activities.decryption_failure_activities.size() > 0) {
      std::vector<base::Value> failure_info;
      SetDecryptionFailureInfo(
          stats->recorded_activities.decryption_failure_activities,
          &failure_info);
      results->SetKey(kDecryptionFailureInfo,
                      base::Value(std::move(failure_info)));
    }
  }
}

}  // namespace gcm_driver
