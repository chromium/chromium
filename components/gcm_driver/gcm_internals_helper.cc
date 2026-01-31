// Copyright 2015 The Chromium Authors
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

base::ListValue CheckinInfoToList(
    const std::vector<gcm::CheckinActivity>& checkins) {
  base::ListValue checkin_info;
  for (const gcm::CheckinActivity& checkin : checkins) {
    base::ListValue row;
    row.Append(checkin.time.InMillisecondsFSinceUnixEpoch());
    row.Append(checkin.event);
    row.Append(checkin.details);
    checkin_info.Append(std::move(row));
  }
  return checkin_info;
}

base::ListValue ConnectionInfoToList(
    const std::vector<gcm::ConnectionActivity>& connections) {
  base::ListValue connection_info;
  for (const gcm::ConnectionActivity& connection : connections) {
    base::ListValue row;
    row.Append(connection.time.InMillisecondsFSinceUnixEpoch());
    row.Append(connection.event);
    row.Append(connection.details);
    connection_info.Append(std::move(row));
  }
  return connection_info;
}

base::ListValue RegistrationInfoToList(
    const std::vector<gcm::RegistrationActivity>& registrations) {
  base::ListValue registration_info;
  for (const gcm::RegistrationActivity& registration : registrations) {
    base::ListValue row;
    row.Append(registration.time.InMillisecondsFSinceUnixEpoch());
    row.Append(registration.app_id);
    row.Append(registration.source);
    row.Append(registration.event);
    row.Append(registration.details);
    registration_info.Append(std::move(row));
  }
  return registration_info;
}

base::ListValue ReceivingInfoToList(
    const std::vector<gcm::ReceivingActivity>& receives) {
  base::ListValue receive_info;
  for (const gcm::ReceivingActivity& receive : receives) {
    base::ListValue row;
    row.Append(receive.time.InMillisecondsFSinceUnixEpoch());
    row.Append(receive.app_id);
    row.Append(receive.from);
    row.Append(base::NumberToString(receive.message_byte_size));
    row.Append(receive.event);
    row.Append(receive.details);
    receive_info.Append(std::move(row));
  }
  return receive_info;
}

base::ListValue SendingInfoToList(
    const std::vector<gcm::SendingActivity>& sends) {
  base::ListValue send_info;
  for (const gcm::SendingActivity& send : sends) {
    base::ListValue row;
    row.Append(send.time.InMillisecondsFSinceUnixEpoch());
    row.Append(send.app_id);
    row.Append(send.receiver_id);
    row.Append(send.message_id);
    row.Append(send.event);
    row.Append(send.details);
    send_info.Append(std::move(row));
  }
  return send_info;
}

base::ListValue DecryptionFailureInfoToList(
    const std::vector<gcm::DecryptionFailureActivity>& failures) {
  base::ListValue failure_info;
  for (const gcm::DecryptionFailureActivity& failure : failures) {
    base::ListValue row;
    row.Append(failure.time.InMillisecondsFSinceUnixEpoch());
    row.Append(failure.app_id);
    row.Append(failure.details);
    failure_info.Append(std::move(row));
  }
  return failure_info;
}

}  // namespace

base::DictValue SetGCMInternalsInfo(const gcm::GCMClient::GCMStatistics* stats,
                                    gcm::GCMProfileService* profile_service,
                                    PrefService* prefs) {
  base::DictValue results;

  if (stats) {
    results.Set(kIsRecording, stats->is_recording);

    base::DictValue device_info;
    device_info.Set(kProfileServiceCreated, profile_service != nullptr);
    device_info.Set(kGcmEnabled, true);
    device_info.Set(kGcmClientCreated, stats->gcm_client_created);
    device_info.Set(kGcmClientState, stats->gcm_client_state);
    device_info.Set(kConnectionClientCreated, stats->connection_client_created);

    base::ListValue registered_app_ids;
    for (const std::string& app_id : stats->registered_app_ids)
      registered_app_ids.Append(app_id);

    device_info.Set(kRegisteredAppIds, std::move(registered_app_ids));

    if (stats->connection_client_created)
      device_info.Set(kConnectionState, stats->connection_state);
    if (!stats->last_checkin.is_null()) {
      device_info.Set(kLastCheckin,
                      base::UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(
                          stats->last_checkin)));
    }
    if (!stats->next_checkin.is_null()) {
      device_info.Set(kNextCheckin,
                      base::UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(
                          stats->next_checkin)));
    }
    if (stats->android_id > 0) {
      device_info.Set(kAndroidId,
                      base::StringPrintf("0x%" PRIx64, stats->android_id));
    }
    if (stats->android_secret > 0) {
      device_info.Set(kAndroidSecret,
                      base::NumberToString(stats->android_secret));
    }
    device_info.Set(kSendQueueSize, stats->send_queue_size);
    device_info.Set(kResendQueueSize, stats->resend_queue_size);
    results.Set(kDeviceInfo, std::move(device_info));

    if (stats->recorded_activities.checkin_activities.size() > 0) {
      results.Set(
          kCheckinInfo,
          CheckinInfoToList(stats->recorded_activities.checkin_activities));
    }
    if (stats->recorded_activities.connection_activities.size() > 0) {
      results.Set(kConnectionInfo,
                  ConnectionInfoToList(
                      stats->recorded_activities.connection_activities));
    }
    if (stats->recorded_activities.registration_activities.size() > 0) {
      results.Set(kRegistrationInfo,
                  RegistrationInfoToList(
                      stats->recorded_activities.registration_activities));
    }
    if (stats->recorded_activities.receiving_activities.size() > 0) {
      results.Set(
          kReceiveInfo,
          ReceivingInfoToList(stats->recorded_activities.receiving_activities));
    }
    if (stats->recorded_activities.sending_activities.size() > 0) {
      results.Set(
          kSendInfo,
          SendingInfoToList(stats->recorded_activities.sending_activities));
    }

    if (stats->recorded_activities.decryption_failure_activities.size() > 0) {
      results.Set(
          kDecryptionFailureInfo,
          DecryptionFailureInfoToList(
              stats->recorded_activities.decryption_failure_activities));
    }
  }
  return results;
}

}  // namespace gcm_driver
