// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/send_tab_to_self_metrics.h"

#include "base/metrics/histogram_macros.h"

namespace send_tab_to_self {

const char kNotificationStatusHistogram[] = "SendTabToSelf.Notification";

void RecordNotificationHistogram(SendTabToSelfNotification status) {
  UMA_HISTOGRAM_ENUMERATION(kNotificationStatusHistogram, status);
}

}  // namespace send_tab_to_self
