// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/metrics_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace send_tab_to_self {

namespace {

// Status of received STTS notifications.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Keep in sync with SendTabToSelfNotificationStatus in enums.xml.
enum class NotificationStatus {
  kShown = 0,
  kDismissed = 1,
  kOpened = 2,
  kTimedOut = 3,
  // kSent = 4,
  kDismissReasonUnknown = 5,
  kThrottled = 6,
  kMaxValue = kThrottled,
};

}  // namespace

void RecordNotificationShown() {
  base::UmaHistogramEnumeration("Sharing.SendTabToSelf.NotificationStatus",
                                NotificationStatus::kShown);
}

void RecordNotificationDismissed() {
  base::UmaHistogramEnumeration("Sharing.SendTabToSelf.NotificationStatus",
                                NotificationStatus::kDismissed);
}

void RecordNotificationOpened() {
  base::UmaHistogramEnumeration("Sharing.SendTabToSelf.NotificationStatus",
                                NotificationStatus::kOpened);
}

void RecordNotificationTimedOut() {
  base::UmaHistogramEnumeration("Sharing.SendTabToSelf.NotificationStatus",
                                NotificationStatus::kTimedOut);
}

void RecordNotificationDismissReasonUnknown() {
  base::UmaHistogramEnumeration("Sharing.SendTabToSelf.NotificationStatus",
                                NotificationStatus::kDismissReasonUnknown);
}

void RecordNotificationThrottled() {
  base::UmaHistogramEnumeration("Sharing.SendTabToSelf.NotificationStatus",
                                NotificationStatus::kThrottled);
}

}  // namespace send_tab_to_self
