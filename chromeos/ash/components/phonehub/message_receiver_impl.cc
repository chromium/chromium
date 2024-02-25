// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/message_receiver_impl.h"

#include <netinet/in.h>
#include <stdint.h>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/logging.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "chromeos/ash/components/phonehub/util/histogram_util.h"

namespace ash::phonehub {

namespace {

std::string GetMessageTypeName(proto::MessageType message_type) {
  switch (message_type) {
    case proto::MessageType::PHONE_STATUS_SNAPSHOT:
      return "PHONE_STATUS_SNAPSHOT";
    case proto::MessageType::PHONE_STATUS_UPDATE:
      return "PHONE_STATUS_UPDATE";
    case proto::MessageType::UPDATE_NOTIFICATION_MODE_RESPONSE:
      return "UPDATE_NOTIFICATION_MODE_RESPONSE";
    case proto::MessageType::RING_DEVICE_RESPONSE:
      return "RING_DEVICE_RESPONSE";
    case proto::MessageType::UPDATE_BATTERY_MODE_RESPONSE:
      return "UPDATE_BATTERY_MODE_RESPONSE";
    case proto::MessageType::DISMISS_NOTIFICATION_RESPONSE:
      return "DISMISS_NOTIFICATION_RESPONSE";
    case proto::MessageType::NOTIFICATION_INLINE_REPLY_RESPONSE:
      return "NOTIFICATION_INLINE_REPLY_RESPONSE";
    case proto::MessageType::SHOW_NOTIFICATION_ACCESS_SETUP_RESPONSE:
      return "SHOW_NOTIFICATION_ACCESS_SETUP_RESPONSE";
    case proto::MessageType::FETCH_CAMERA_ROLL_ITEMS_RESPONSE:
      return "FETCH_CAMERA_ROLL_ITEMS_RESPONSE";
    case proto::MessageType::FETCH_CAMERA_ROLL_ITEM_DATA_RESPONSE:
      return "FETCH_CAMERA_ROLL_ITEM_DATA_RESPONSE";
    case proto::MessageType::FEATURE_SETUP_RESPONSE:
      return "FEATURE_SETUP_RESPONSE";
    case proto::MessageType::PING_RESPONSE:
      return "PING_RESPONSE";
    case proto::MessageType::APP_STREAM_UPDATE:
      return "APP_STREAM_UPDATE";
    case proto::MessageType::APP_LIST_UPDATE:
      return "APP_LIST_UPDATE";
    case proto::MessageType::APP_LIST_INCREMENTAL_UPDATE:
      return "APP_LIST_INCREMENTAL_UPDATE";
    default:
      return "UNKOWN_MESSAGE";
  }
}

}  // namespace

MessageReceiverImpl::MessageReceiverImpl(
    secure_channel::ConnectionManager* connection_manager,
    PhoneHubStructuredMetricsLogger* phone_hub_structured_metrics_logger)
    : connection_manager_(connection_manager),
      phone_hub_structured_metrics_logger_(
          phone_hub_structured_metrics_logger) {
  DCHECK(connection_manager_);

  connection_manager_->AddObserver(this);
}

MessageReceiverImpl::~MessageReceiverImpl() {
  connection_manager_->RemoveObserver(this);
}

void MessageReceiverImpl::OnMessageReceived(const std::string& payload) {
  // The first two bytes of |payload| is reserved for the header
  // proto::MessageType.
  uint16_t* ptr =
      reinterpret_cast<uint16_t*>(const_cast<char*>(payload.data()));
  proto::MessageType message_type =
      static_cast<proto::MessageType>(ntohs(*ptr));

  PA_LOG(INFO) << "MessageReceiver received a "
               << GetMessageTypeName(message_type) << " message.";
  util::LogMessageResult(message_type,
                         util::PhoneHubMessageResult::kResponseReceived);
  phone_hub_structured_metrics_logger_->LogPhoneHubMessageEvent(
      message_type, PhoneHubMessageDirection::kPhoneToChromebook);

  // Decode the proto message if the message is something we want to notify to
  // clients.
  if (message_type == proto::MessageType::PHONE_STATUS_SNAPSHOT) {
    proto::PhoneStatusSnapshot snapshot_proto;
    // Serialized proto is after the first two bytes of |payload|.
    if (!snapshot_proto.ParseFromString(payload.substr(2))) {
      PA_LOG(ERROR) << "OnMessageReceived() could not deserialize the "
                    << "PhoneStatusSnapshot proto message.";
      return;
    }
    NotifyPhoneStatusSnapshotReceived(snapshot_proto);
    return;
  }

  if (message_type == proto::MessageType::PHONE_STATUS_UPDATE) {
    proto::PhoneStatusUpdate update_proto;
    // Serialized proto is after the first two bytes of |payload|.
    if (!update_proto.ParseFromString(payload.substr(2))) {
      PA_LOG(ERROR) << "OnMessageReceived() could not deserialize the "
                    << "PhoneStatusUpdate proto message.";
      return;
    }
    NotifyPhoneStatusUpdateReceived(update_proto);
    return;
  }

  if (features::IsPhoneHubFeatureSetupErrorHandlingEnabled() &&
      message_type == proto::MessageType::FEATURE_SETUP_RESPONSE) {
    proto::FeatureSetupResponse response;
    // Serialized proto is after the first two bytes of |payload|.
    if (!response.ParseFromString(payload.substr(2))) {
      PA_LOG(ERROR) << "OnMessageReceived() could not deserialize the "
                    << "FeatureSetupResponse proto message.";
      return;
    }
    NotifyFeatureSetupResponseReceived(response);
  }

  if (features::IsPhoneHubCameraRollEnabled() &&
      message_type == proto::MessageType::FETCH_CAMERA_ROLL_ITEMS_RESPONSE) {
    proto::FetchCameraRollItemsResponse response;
    // Serialized proto is after the first two bytes of |payload|.
    if (!response.ParseFromString(payload.substr(2))) {
      PA_LOG(ERROR) << "OnMessageReceived() could not deserialize the "
                    << "FetchCameraRollItemsResponse proto message.";
      return;
    }
    NotifyFetchCameraRollItemsResponseReceived(response);
    return;
  }

  if (features::IsPhoneHubCameraRollEnabled() &&
      message_type ==
          proto::MessageType::FETCH_CAMERA_ROLL_ITEM_DATA_RESPONSE) {
    proto::FetchCameraRollItemDataResponse response;
    // Serialized proto is after the first two bytes of |payload|.
    if (!response.ParseFromString(payload.substr(2))) {
      PA_LOG(ERROR) << "OnMessageReceived() could not deserialize the "
                    << "FetchCameraRollItemDataResponse proto message.";
      return;
    }
    NotifyFetchCameraRollItemDataResponseReceived(response);
    return;
  }

  if (features::IsPhoneHubPingOnBubbleOpenEnabled() &&
      message_type == proto::MessageType::PING_RESPONSE) {
    // We don't need to send the content of the ping response, we only care
    // that we got a response.
    NotifyPingResponseReceived();
    return;
  }

  if (features::IsEcheSWAEnabled() &&
      message_type == proto::MessageType::APP_STREAM_UPDATE) {
    proto::AppStreamUpdate app_stream_update;
    // Serialized proto is after the first two bytes of |payload|.
    if (!app_stream_update.ParseFromString(payload.substr(2))) {
      PA_LOG(ERROR) << "OnMessageReceived() could not deserialize the "
                    << "AppStreamUpdate proto message.";
      return;
    }
    NotifyAppStreamUpdateReceived(app_stream_update);
    return;
  }

  if (features::IsEcheSWAEnabled() &&
      message_type == proto::MessageType::APP_LIST_UPDATE) {
    proto::AppListUpdate app_list_update;
    // Serialized proto is after the first two bytes of |payload|.
    if (!app_list_update.ParseFromString(payload.substr(2))) {
      PA_LOG(ERROR) << "OnMessageReceived() could not deserialize the "
                    << "AppListUpdate proto message.";
      return;
    }
    NotifyAppListUpdateReceived(app_list_update);
    return;
  }

  if (features::IsEcheSWAEnabled() &&
      message_type == proto::MessageType::APP_LIST_INCREMENTAL_UPDATE) {
    proto::AppListIncrementalUpdate app_list_incrementalUpdate;
    if (!app_list_incrementalUpdate.ParseFromString(payload.substr(2))) {
      PA_LOG(ERROR) << "OnMessageReceived() could not deserialize the "
                    << "AppListIncrementalUpdate proto message.";
      return;
    }
    NotifyAppListIncrementalUpdateReceived(app_list_incrementalUpdate);
    return;
  }
}

}  // namespace ash::phonehub
