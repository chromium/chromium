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
                    base::ListValue* checkin_info) {
  for (const gcm::CheckinActivity& checkin : checkins) {
    auto row = std::make_unique<base::ListValue>();
    row->Append(checkin.time.ToJsTime());
    row->AppendString(checkin.event);
    row->AppendString(checkin.details);
    checkin_info->Append(std::move(row));
  }
}

void SetConnectionInfo(const std::vector<gcm::ConnectionActivity>& connections,
                       base::ListValue* connection_info) {
  for (const gcm::ConnectionActivity& connection : connections) {
    auto row = std::make_unique<base::ListValue>();
    row->Append(connection.time.ToJsTime());
    row->AppendString(connection.event);
    row->AppendString(connection.details);
    connection_info->Append(std::move(row));
  }
}

void SetRegistrationInfo(
    const std::vector<gcm::RegistrationActivity>& registrations,
    base::ListValue* registration_info) {
  for (const gcm::RegistrationActivity& registration : registrations) {
    auto row = std::make_unique<base::ListValue>();
    row->Append(registration.time.ToJsTime());
    row->AppendString(registration.app_id);
    row->AppendString(registration.source);
    row->AppendString(registration.event);
    row->AppendString(registration.details);
    registration_info->Append(std::move(row));
  }
}

void SetReceivingInfo(const std::vector<gcm::ReceivingActivity>& receives,
                      base::ListValue* receive_info) {
  for (const gcm::ReceivingActivity& receive : receives) {
    auto row = std::make_unique<base::ListValue>();
    row->Append(receive.time.ToJsTime());
    row->AppendString(receive.app_id);
    row->AppendString(receive.from);
    row->AppendString(base::NumberToString(receive.message_byte_size));
    row->AppendString(receive.event);
    row->AppendString(receive.details);
    receive_info->Append(std::move(row));
  }
}

void SetSendingInfo(const std::vector<gcm::SendingActivity>& sends,
                    base::ListValue* send_info) {
  for (const gcm::SendingActivity& send : sends) {
    auto row = std::make_unique<base::ListValue>();
    row->Append(send.time.ToJsTime());
    row->AppendString(send.app_id);
    row->AppendString(send.receiver_id);
    row->AppendString(send.message_id);
    row->AppendString(send.event);
    row->AppendString(send.details);
    send_info->Append(std::move(row));
  }
}

void SetDecryptionFailureInfo(
    const std::vector<gcm::DecryptionFailureActivity>& failures,
    base::ListValue* failure_info) {
  for (const gcm::DecryptionFailureActivity& failure : failures) {
    auto row = std::make_unique<base::ListValue>();
    row->Append(failure.time.ToJsTime());
    row->AppendString(failure.app_id);
    row->AppendString(failure.details);
    failure_info->Append(std::move(row));
  }
}

}  // namespace

void SetGCMInternalsInfo(const gcm::GCMClient::GCMStatistics* stats,
                         gcm::GCMProfileService* profile_service,
                         PrefService* prefs,
                         base::DictionaryValue* results) {
  auto device_info = std::make_unique<base::DictionaryValue>();

  device_info->SetBoolean(kProfileServiceCreated, profile_service != nullptr);
  device_info->SetBoolean(kGcmEnabled, true);
  if (stats) {
    results->SetBoolean(kIsRecording, stats->is_recording);
    device_info->SetBoolean(kGcmClientCreated, stats->gcm_client_created);
    device_info->SetString(kGcmClientState, stats->gcm_client_state);
    device_info->SetBoolean(kConnectionClientCreated,
                            stats->connection_client_created);

    auto registered_app_ids = std::make_unique<base::ListValue>();
    for (const std::string& app_id : stats->registered_app_ids)
      registered_app_ids->AppendString(app_id);

    device_info->SetList(kRegisteredAppIds, std::move(registered_app_ids));

    if (stats->connection_client_created)
      device_info->SetString(kConnectionState, stats->connection_state);
    if (!stats->last_checkin.is_null()) {
      device_info->SetString(
          kLastCheckin, base::UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(
                            stats->last_checkin)));
    }
    if (!stats->next_checkin.is_null()) {
      device_info->SetString(
          kNextCheckin, base::UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(
                            stats->next_checkin)));
    }
    if (stats->android_id > 0) {
      device_info->SetString(
          kAndroidId, base::StringPrintf("0x%" PRIx64, stats->android_id));
    }
    if (stats->android_secret > 0) {
      device_info->SetString(kAndroidSecret,
                             base::NumberToString(stats->android_secret));
    }
    device_info->SetInteger(kSendQueueSize, stats->send_queue_size);
    device_info->SetInteger(kResendQueueSize, stats->resend_queue_size);
    results->Set(kDeviceInfo, std::move(device_info));

    if (stats->recorded_activities.checkin_activities.size() > 0) {
      auto checkin_info = std::make_unique<base::ListValue>();
      SetCheckinInfo(stats->recorded_activities.checkin_activities,
                     checkin_info.get());
      results->Set(kCheckinInfo, std::move(checkin_info));
    }
    if (stats->recorded_activities.connection_activities.size() > 0) {
      auto connection_info = std::make_unique<base::ListValue>();
      SetConnectionInfo(stats->recorded_activities.connection_activities,
                        connection_info.get());
      results->Set(kConnectionInfo, std::move(connection_info));
    }
    if (stats->recorded_activities.registration_activities.size() > 0) {
      auto registration_info = std::make_unique<base::ListValue>();
      SetRegistrationInfo(stats->recorded_activities.registration_activities,
                          registration_info.get());
      results->Set(kRegistrationInfo, std::move(registration_info));
    }
    if (stats->recorded_activities.receiving_activities.size() > 0) {
      auto receive_info = std::make_unique<base::ListValue>();
      SetReceivingInfo(stats->recorded_activities.receiving_activities,
                       receive_info.get());
      results->Set(kReceiveInfo, std::move(receive_info));
    }
    if (stats->recorded_activities.sending_activities.size() > 0) {
      auto send_info = std::make_unique<base::ListValue>();
      SetSendingInfo(stats->recorded_activities.sending_activities,
                     send_info.get());
      results->Set(kSendInfo, std::move(send_info));
    }

    if (stats->recorded_activities.decryption_failure_activities.size() > 0) {
      auto failure_info = std::make_unique<base::ListValue>();
      SetDecryptionFailureInfo(
          stats->recorded_activities.decryption_failure_activities,
          failure_info.get());
      results->Set(kDecryptionFailureInfo, std::move(failure_info));
    }
  }
}

}  // namespace gcm_driver
