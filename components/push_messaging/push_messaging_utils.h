// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PUSH_MESSAGING_PUSH_MESSAGING_UTILS_H_
#define COMPONENTS_PUSH_MESSAGING_PUSH_MESSAGING_UTILS_H_

#include <optional>
#include <string>

#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging.mojom.h"

class GURL;

namespace version_info {
enum class Channel;
}  // namespace version_info

namespace push_messaging {

// Returns the appropriate GCM endpoint for the given Chrome |channel|.
std::string GetGcmEndpointForChannel(version_info::Channel channel);

// Returns the URL used to send push messages to the subscription identified
// by |subscription_id|.
GURL CreateEndpoint(version_info::Channel channel,
                    const std::string& subscription_id);

// Checks size and prefix to determine whether it is a VAPID key
bool IsVapidKey(const std::string& application_server_key);

// Normalizes the |sender_info|. In most cases the |sender_info| will be
// passed through to the GCM Driver as-is, but NIST P-256 application server
// keys have to be encoded using the URL-safe variant of the base64 encoding.
std::string NormalizeSenderInfo(const std::string& sender_info);

// Currently |user_visible_only| is always true, once silent pushes are
// enabled, get this information from SW database.
blink::mojom::PushSubscriptionOptionsPtr MakeOptions(
    const std::string& sender_id);

// Inspects the |PushEventStatus| and returns if push was successful; when
// returning false, if the std::optional<PushUnregistrationReason> is not
// std::nullopt, it's expected to unregister the subscription.
bool WasPushSuccessful(
    blink::mojom::PushEventStatus status,
    std::optional<blink::mojom::PushUnregistrationReason>& unsubscribe_reason);

}  // namespace push_messaging

#endif  // COMPONENTS_PUSH_MESSAGING_PUSH_MESSAGING_UTILS_H_
