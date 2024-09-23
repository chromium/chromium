// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_SHARING_METRICS_H_
#define COMPONENTS_SHARING_MESSAGE_SHARING_METRICS_H_

#include "base/time/time.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/proto/sharing_message_type.pb.h"
#include "components/sharing_message/shared_clipboard/remote_copy_handle_message_result.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sharing_message/sharing_send_message_result.h"
#include "components/sync/protocol/unencrypted_sharing_message.pb.h"

enum class SharingDeviceRegistrationResult;

// The types of dialogs that can be shown for sharing features.
// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "SharingDialogType" in src/tools/metrics/histograms/enums.xml.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.sharing
enum class SharingDialogType {
  kDialogWithDevicesMaybeApps = 0,
  kDialogWithoutDevicesWithApp = 1,
  kEducationalDialog = 2,
  kErrorDialog = 3,
  kMaxValue = kErrorDialog,
};

// These histogram suffixes must match the ones in Sharing{feature}Ui
// defined in histograms.xml.
const char kSharingUiContextMenu[] = "ContextMenu";
const char kSharingUiDialog[] = "Dialog";

// Maps SharingSendMessageResult enums to strings used as histogram suffixes.
// Keep in sync with "SharingSendMessageResult" in histograms.xml.
std::string SharingSendMessageResultToString(SharingSendMessageResult result);

// Maps PayloadCase enums to MessageType enums.
sharing_message::MessageType SharingPayloadCaseToMessageType(
    components_sharing_message::SharingMessage::PayloadCase payload_case);

// Maps PayloadCase enums to MessageType enums for unencrypted sharing messages.
sharing_message::MessageType SharingPayloadCaseToMessageType(
    sync_pb::UnencryptedSharingMessage::PayloadCase payload_case);

// Maps MessageType enums to strings used as histogram suffixes. Keep in sync
// with "SharingMessage" in histograms.xml.
const std::string& SharingMessageTypeToString(
    sharing_message::MessageType message_type);

// Generates trace ids for async traces in the "sharing" category.
int GenerateSharingTraceId();

// Logs the |payload_case| to UMA. This should be called when a SharingMessage
// is received.
void LogSharingMessageReceived(
    components_sharing_message::SharingMessage::PayloadCase payload_case);

// Logs the |result| to UMA. This should be called after attempting register
// Sharing.
void LogSharingRegistrationResult(SharingDeviceRegistrationResult result);

// Logs the |result| to UMA. This should be called after attempting un-register
// Sharing.
void LogSharingUnregistrationResult(SharingDeviceRegistrationResult result);

// Logs the number of available devices that are about to be shown in a UI for
// picking a device to start a sharing functionality. The |histogram_suffix|
// indicates in which UI this event happened and must match one from
// Sharing{feature}Ui defined in histograms.xml use the constants defined
// in this file for that.
// TODO(yasmo): Change histogram_suffix to be an enum type.
void LogSharingDevicesToShow(SharingFeatureName feature,
                             const char* histogram_suffix,
                             int count);

// Logs the number of available apps that are about to be shown in a UI for
// picking an app to start a sharing functionality. The |histogram_suffix|
// indicates in which UI this event happened and must match one from
// Sharing{feature}Ui defined in histograms.xml - use the constants defined
// in this file for that.
void LogSharingAppsToShow(SharingFeatureName feature,
                          const char* histogram_suffix,
                          int count);

// Logs the |index| of the user selection for sharing feature. |index_type| is
// the type of selection made, either "Device" or "App". The |histogram_suffix|
// indicates in which UI this event happened and must match one from
// Sharing{feature}Ui defined in histograms.xml - use the constants defined in
// this file for that.
enum class SharingIndexType {
  kDevice,
  kApp,
};
void LogSharingSelectedIndex(
    SharingFeatureName feature,
    const char* histogram_suffix,
    int index,
    SharingIndexType index_type = SharingIndexType::kDevice);

// Logs to UMA the time from receiving a SharingMessage to sending
// back an ack.
void LogSharingMessageHandlerTime(sharing_message::MessageType message_type,
                                  base::TimeDelta time_taken);

// Logs to UMA the |type| of dialog shown for sharing feature.
void LogSharingDialogShown(SharingFeatureName feature, SharingDialogType type);

// Logs to UMA result of sending a SharingMessage. This should not be called for
// sending ack messages.
void LogSendSharingMessageResult(sharing_message::MessageType message_type,
                                 SharingDevicePlatform receiver_device_platform,
                                 SharingChannelType channel_type,
                                 base::TimeDelta receiver_pulse_interval,
                                 SharingSendMessageResult result);

#endif  // COMPONENTS_SHARING_MESSAGE_SHARING_METRICS_H_
