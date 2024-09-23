// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/sharing_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/sharing_message/sharing_device_registration_result.h"
#include "components/version_info/version_info.h"

namespace {
const char* GetEnumStringValue(SharingFeatureName feature) {
  DCHECK(feature != SharingFeatureName::kUnknown)
      << "Feature needs to be specified for metrics logging.";

  switch (feature) {
    case SharingFeatureName::kUnknown:
      return "Unknown";
    case SharingFeatureName::kClickToCall:
      return "ClickToCall";
    case SharingFeatureName::kSharedClipboard:
      return "SharedClipboard";
    case SharingFeatureName::kSmsRemoteFetcher:
      return "SmsRemoteFetcher";
  }
}

// Maps SharingChannelType enum values to strings used as histogram
// suffixes. Keep in sync with "SharingChannelType" in histograms.xml.
std::string SharingChannelTypeToString(SharingChannelType channel_type) {
  switch (channel_type) {
    case SharingChannelType::kUnknown:
      return "Unknown";
    case SharingChannelType::kFcmVapid:
      return "FcmVapid";
    case SharingChannelType::kFcmSenderId:
      return "FcmSenderId";
    case SharingChannelType::kServer:
      return "Server";
    case SharingChannelType::kWebRtc:
      return "WebRTC";
    case SharingChannelType::kIosPush:
      return "IosPush";
  }
}

// Maps SharingDevicePlatform enum values to strings used as histogram
// suffixes. Keep in sync with "SharingDevicePlatform" in histograms.xml.
std::string DevicePlatformToString(SharingDevicePlatform device_platform) {
  switch (device_platform) {
    case SharingDevicePlatform::kAndroid:
      return "Android";
    case SharingDevicePlatform::kChromeOS:
      return "ChromeOS";
    case SharingDevicePlatform::kIOS:
      return "iOS";
    case SharingDevicePlatform::kLinux:
      return "Linux";
    case SharingDevicePlatform::kMac:
      return "Mac";
    case SharingDevicePlatform::kWindows:
      return "Windows";
    case SharingDevicePlatform::kServer:
      return "Server";
    case SharingDevicePlatform::kUnknown:
      return "Unknown";
  }
}

// Maps pulse intervals to strings used as histogram suffixes. Keep in sync with
// "SharingPulseInterval" in histograms.xml.
std::string PulseIntervalToString(base::TimeDelta pulse_interval) {
  if (pulse_interval < base::Hours(4)) {
    return "PulseIntervalShort";
  }
  if (pulse_interval > base::Hours(12)) {
    return "PulseIntervalLong";
  }
  return "PulseIntervalMedium";
}

// Major Chrome version comparison with the receiver device.
// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "SharingMajorVersionComparison" in enums.xml.
enum class SharingMajorVersionComparison {
  kUnknown = 0,
  kSenderIsLower = 1,
  kSame = 2,
  kSenderIsHigher = 3,
  kMaxValue = kSenderIsHigher,
};
}  // namespace

std::string SharingSendMessageResultToString(SharingSendMessageResult result) {
  switch (result) {
    case SharingSendMessageResult::kSuccessful:
      return "Successful";
    case SharingSendMessageResult::kDeviceNotFound:
      return "DeviceNotFound";
    case SharingSendMessageResult::kNetworkError:
      return "NetworkError";
    case SharingSendMessageResult::kPayloadTooLarge:
      return "PayloadTooLarge";
    case SharingSendMessageResult::kAckTimeout:
      return "AckTimeout";
    case SharingSendMessageResult::kInternalError:
      return "InternalError";
    case SharingSendMessageResult::kEncryptionError:
      return "EncryptionError";
    case SharingSendMessageResult::kCommitTimeout:
      return "CommitTimeout";
    case SharingSendMessageResult::kCancelled:
      return "RequestCancelled";
  }
}

sharing_message::MessageType SharingPayloadCaseToMessageType(
    components_sharing_message::SharingMessage::PayloadCase payload_case) {
  switch (payload_case) {
    case components_sharing_message::SharingMessage::PAYLOAD_NOT_SET:
      return sharing_message::UNKNOWN_MESSAGE;
    case components_sharing_message::SharingMessage::kPingMessage:
      return sharing_message::PING_MESSAGE;
    case components_sharing_message::SharingMessage::kAckMessage:
      return sharing_message::ACK_MESSAGE;
    case components_sharing_message::SharingMessage::kClickToCallMessage:
      return sharing_message::CLICK_TO_CALL_MESSAGE;
    case components_sharing_message::SharingMessage::kSharedClipboardMessage:
      return sharing_message::SHARED_CLIPBOARD_MESSAGE;
    case components_sharing_message::SharingMessage::kSmsFetchRequest:
      return sharing_message::SMS_FETCH_REQUEST;
    case components_sharing_message::SharingMessage::kRemoteCopyMessage:
      return sharing_message::REMOTE_COPY_MESSAGE;
    case components_sharing_message::SharingMessage::
        kPeerConnectionOfferMessage:
      return sharing_message::PEER_CONNECTION_OFFER_MESSAGE;
    case components_sharing_message::SharingMessage::
        kPeerConnectionIceCandidatesMessage:
      return sharing_message::PEER_CONNECTION_ICE_CANDIDATES_MESSAGE;
    case components_sharing_message::SharingMessage::kDiscoveryRequest:
      return sharing_message::DISCOVERY_REQUEST;
    case components_sharing_message::SharingMessage::kWebRtcSignalingFrame:
      return sharing_message::WEB_RTC_SIGNALING_FRAME;
    case components_sharing_message::SharingMessage::
        kOptimizationGuidePushNotification:
      return sharing_message::OPTIMIZATION_GUIDE_PUSH_NOTIFICATION;
  }
  // For proto3 enums unrecognized enum values are kept when parsing, and a new
  // payload case received over the network would not default to
  // PAYLOAD_NOT_SET. Explicitly return UNKNOWN_MESSAGE here to handle this
  // case.
  return sharing_message::UNKNOWN_MESSAGE;
}

sharing_message::MessageType SharingPayloadCaseToMessageType(
    sync_pb::UnencryptedSharingMessage::PayloadCase payload_case) {
  switch (payload_case) {
    case sync_pb::UnencryptedSharingMessage::PAYLOAD_NOT_SET:
      return sharing_message::UNKNOWN_MESSAGE;
    case sync_pb::UnencryptedSharingMessage::kSendTabMessage:
      return sharing_message::SEND_TAB_TO_SELF_PUSH_NOTIFICATION;
  }
  // For proto3 enums unrecognized enum values are kept when parsing, and a new
  // payload case received over the network would not default to
  // PAYLOAD_NOT_SET. Explicitly return UNKNOWN_MESSAGE here to handle this
  // case.
  return sharing_message::UNKNOWN_MESSAGE;
}

const std::string& SharingMessageTypeToString(
    sharing_message::MessageType message_type) {
  // For proto3 enums unrecognized enum values are kept when parsing and their
  // name is an empty string. We don't want to use that as a histogram suffix.
  if (!sharing_message::MessageType_IsValid(message_type)) {
    return sharing_message::MessageType_Name(sharing_message::UNKNOWN_MESSAGE);
  }
  return sharing_message::MessageType_Name(message_type);
}

int GenerateSharingTraceId() {
  static int next_id = 0;
  return next_id++;
}

void LogSharingMessageReceived(
    components_sharing_message::SharingMessage::PayloadCase payload_case) {
  base::UmaHistogramExactLinear("Sharing.MessageReceivedType",
                                SharingPayloadCaseToMessageType(payload_case),
                                sharing_message::MessageType_ARRAYSIZE);
}

void LogSharingDevicesToShow(SharingFeatureName feature,
                             const char* histogram_suffix,
                             int count) {
  auto* feature_str = GetEnumStringValue(feature);
  // Explicitly log both the base and the suffixed histogram because the base
  // aggregation is not automatically generated.
  base::UmaHistogramExactLinear(
      base::StrCat({"Sharing.", feature_str, "DevicesToShow"}), count,
      /*value_max=*/20);
  if (!histogram_suffix) {
    return;
  }
  base::UmaHistogramExactLinear(
      base::StrCat(
          {"Sharing.", feature_str, "DevicesToShow.", histogram_suffix}),
      count,
      /*value_max=*/20);
}

void LogSharingAppsToShow(SharingFeatureName feature,
                          const char* histogram_suffix,
                          int count) {
  auto* feature_str = GetEnumStringValue(feature);
  // Explicitly log both the base and the suffixed histogram because the base
  // aggregation is not automatically generated.
  base::UmaHistogramExactLinear(
      base::StrCat({"Sharing.", feature_str, "AppsToShow"}), count,
      /*value_max=*/20);
  if (!histogram_suffix) {
    return;
  }
  base::UmaHistogramExactLinear(
      base::StrCat({"Sharing.", feature_str, "AppsToShow.", histogram_suffix}),
      count,
      /*value_max=*/20);
}

void LogSharingSelectedIndex(SharingFeatureName feature,
                             const char* histogram_suffix,
                             int index,
                             SharingIndexType index_type) {
  auto* feature_str = GetEnumStringValue(feature);
  // Explicitly log both the base and the suffixed histogram because the base
  // aggregation is not automatically generated.
  std::string name = base::StrCat(
      {"Sharing.", feature_str, "Selected",
       (index_type == SharingIndexType::kDevice) ? "Device" : "App", "Index"});
  base::UmaHistogramExactLinear(name, index, /*value_max=*/20);
  if (!histogram_suffix) {
    return;
  }
  base::UmaHistogramExactLinear(base::StrCat({name, ".", histogram_suffix}),
                                index,
                                /*value_max=*/20);
}

void LogSharingDialogShown(SharingFeatureName feature, SharingDialogType type) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Sharing.", GetEnumStringValue(feature), "DialogShown"}),
      type);
}

void LogSendSharingMessageResult(
    sharing_message::MessageType message_type,
    SharingDevicePlatform receiving_device_platform,
    SharingChannelType channel_type,
    base::TimeDelta pulse_interval,
    SharingSendMessageResult result) {
  const std::string metric_prefix = "Sharing.SendMessageResult";

  base::UmaHistogramEnumeration(metric_prefix, result);

  base::UmaHistogramEnumeration(
      base::StrCat(
          {metric_prefix, ".", SharingMessageTypeToString(message_type)}),
      result);

  base::UmaHistogramEnumeration(
      base::StrCat({metric_prefix, ".",
                    DevicePlatformToString(receiving_device_platform)}),
      result);

  base::UmaHistogramEnumeration(
      base::StrCat({metric_prefix, ".",
                    DevicePlatformToString(receiving_device_platform), ".",
                    SharingMessageTypeToString(message_type)}),
      result);

  base::UmaHistogramEnumeration(
      base::StrCat(
          {metric_prefix, ".", SharingChannelTypeToString(channel_type)}),
      result);

  // There is no "invalid" bucket so only log valid pulse intervals.
  if (!pulse_interval.is_zero()) {
    base::UmaHistogramEnumeration(
        base::StrCat(
            {metric_prefix, ".", PulseIntervalToString(pulse_interval)}),
        result);
    base::UmaHistogramEnumeration(
        base::StrCat({metric_prefix, ".",
                      DevicePlatformToString(receiving_device_platform), ".",
                      PulseIntervalToString(pulse_interval)}),
        result);
  }
}
