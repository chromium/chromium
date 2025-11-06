// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/push_messaging/push_messaging_utils.h"

#include "base/base64url.h"
#include "base/version_info/channel.h"
#include "components/push_messaging/push_messaging_constants.h"
#include "components/push_messaging/push_messaging_features.h"
#include "url/gurl.h"

namespace push_messaging {

std::string GetGcmEndpointForChannel(version_info::Channel channel) {
  if (base::FeatureList::IsEnabled(
          features::kPushMessagingGcmEndpointWebpushPath)) {
      if (channel != version_info::Channel::STABLE) {
        return kPushMessagingStagingWebpushEndpoint;
      }
    return kPushMessagingWebpushEndpoint;
  }

    if (channel != version_info::Channel::STABLE) {
      return kPushMessagingStagingGcmEndpoint;
    }

  return kPushMessagingGcmEndpoint;
}

GURL CreateEndpoint(version_info::Channel channel,
                    const std::string& subscription_id) {
  const GURL endpoint(GetGcmEndpointForChannel(channel) + subscription_id);
  DCHECK(endpoint.is_valid());
  return endpoint;
}

blink::mojom::PushSubscriptionOptionsPtr MakeOptions(
    const std::string& sender_id) {
  return blink::mojom::PushSubscriptionOptions::New(
      /*user_visible_only=*/true,
      std::vector<uint8_t>(sender_id.begin(), sender_id.end()));
}

bool IsVapidKey(const std::string& application_server_key) {
  // VAPID keys are NIST P-256 public keys in uncompressed format (64 bytes),
  // verified through its length and the 0x04 prefix.
  return application_server_key.size() == 65 &&
         application_server_key[0] == 0x04;
}

std::string NormalizeSenderInfo(const std::string& application_server_key) {
  if (!IsVapidKey(application_server_key)) {
    return application_server_key;
  }

  std::string encoded_application_server_key;
  base::Base64UrlEncode(application_server_key,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_application_server_key);

  return encoded_application_server_key;
}

bool WasPushSuccessful(
    blink::mojom::PushEventStatus status,
    std::optional<blink::mojom::PushUnregistrationReason>& unsubscribe_reason) {
  switch (status) {
    case blink::mojom::PushEventStatus::SUCCESS:
    case blink::mojom::PushEventStatus::EVENT_WAITUNTIL_REJECTED:
    case blink::mojom::PushEventStatus::TIMEOUT:
      unsubscribe_reason = std::nullopt;
      return true;
    case blink::mojom::PushEventStatus::SERVICE_WORKER_ERROR:
      // Do nothing, and hope the error is transient.
      unsubscribe_reason = std::nullopt;
      break;
    case blink::mojom::PushEventStatus::NO_APP_LEVEL_PERMISSION_IGNORE:
      // Do nothing, ignore push messages during the grace period.
      unsubscribe_reason = std::nullopt;
      break;
    case blink::mojom::PushEventStatus::NO_APP_LEVEL_PERMISSION_UNSUBSCRIBE:
      unsubscribe_reason =
          blink::mojom::PushUnregistrationReason::NO_APP_LEVEL_PERMISSION;
      break;
    case blink::mojom::PushEventStatus::UNKNOWN_APP_ID:
      unsubscribe_reason =
          blink::mojom::PushUnregistrationReason::DELIVERY_UNKNOWN_APP_ID;
      break;
    case blink::mojom::PushEventStatus::PERMISSION_DENIED:
      unsubscribe_reason =
          blink::mojom::PushUnregistrationReason::DELIVERY_PERMISSION_DENIED;
      break;
    case blink::mojom::PushEventStatus::NO_SERVICE_WORKER:
      unsubscribe_reason =
          blink::mojom::PushUnregistrationReason::DELIVERY_NO_SERVICE_WORKER;
      break;
    case blink::mojom::PushEventStatus::PERMISSION_REVOKED_ABUSIVE:
      unsubscribe_reason =
          blink::mojom::PushUnregistrationReason::PERMISSION_REVOKED_ABUSIVE;
      break;
    case blink::mojom::PushEventStatus::PERMISSION_REVOKED_DISRUPTIVE:
      unsubscribe_reason =
          blink::mojom::PushUnregistrationReason::PERMISSION_REVOKED_DISRUPTIVE;
      break;
  }
  return false;
}

}  // namespace push_messaging
