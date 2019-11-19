// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_WEB_PUSH_COMMON_H_
#define COMPONENTS_GCM_DRIVER_WEB_PUSH_COMMON_H_

#include <string>

#include "base/callback.h"
#include "base/optional.h"

namespace gcm {

// Result of sending web push message.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SendWebPushMessageResult {
  kSuccessful = 0,
  kEncryptionFailed = 1,
  kCreateJWTFailed = 2,
  kNetworkError = 3,
  kServerError = 4,
  kParseResponseFailed = 5,
  kVapidKeyInvalid = 6,
  kDeviceGone = 7,
  kPayloadTooLarge = 8,
  kMaxValue = kPayloadTooLarge,
};

using WebPushCallback = base::OnceCallback<void(SendWebPushMessageResult,
                                                base::Optional<std::string>)>;

// Invoke |callback| with |result| and logs the |result| to UMA. This should be
// called when after a web push message is sent. If |result| is
// SendWebPushMessageResult::Successful, |message_id| must be present.
void InvokeWebPushCallback(
    WebPushCallback callback,
    SendWebPushMessageResult result,
    base::Optional<std::string> message_id = base::nullopt);

// Logs the size of message payload to UMA. This should be called right before a
// web push message is sent.
void LogSendWebPushMessagePayloadSize(int size);

// Logs the network error or status code after a web push message is sent.
void LogSendWebPushMessageStatusCode(int status_code);

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_WEB_PUSH_COMMON_H_
