// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/web_push/web_push_common.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"

const int WebPushMessage::kMaximumTTL = 24 * 60 * 60;  // 1 day.

WebPushMessage::WebPushMessage() = default;

WebPushMessage::WebPushMessage(WebPushMessage&& other) = default;

WebPushMessage::~WebPushMessage() = default;

WebPushMessage& WebPushMessage::operator=(WebPushMessage&& other) = default;

void InvokeWebPushCallback(WebPushCallback callback,
                           SendWebPushMessageResult result,
                           std::optional<std::string> message_id) {
  DCHECK(message_id || result != SendWebPushMessageResult::kSuccessful);
  base::UmaHistogramEnumeration("GCM.SendWebPushMessageResult", result);
  std::move(callback).Run(result, std::move(message_id));
}
