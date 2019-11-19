// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/web_push_common.h"

#include "base/metrics/histogram_functions.h"

namespace gcm {

void InvokeWebPushCallback(WebPushCallback callback,
                           SendWebPushMessageResult result,
                           base::Optional<std::string> message_id) {
  DCHECK(message_id || result != SendWebPushMessageResult::kSuccessful);
  base::UmaHistogramEnumeration("GCM.SendWebPushMessageResult", result);
  std::move(callback).Run(result, std::move(message_id));
}

void LogSendWebPushMessagePayloadSize(int size) {
  // Note: The maximum size accepted by FCM is 4096.
  base::UmaHistogramCounts10000("GCM.SendWebPushMessagePayloadSize", size);
}

void LogSendWebPushMessageStatusCode(int status_code) {
  base::UmaHistogramSparse("GCM.SendWebPushMessageStatusCode", status_code);
}

}  // namespace gcm
