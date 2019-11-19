// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_METRICS_H_
#define COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_METRICS_H_

namespace send_tab_to_self {

// Metrics for measuring notification interaction.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
extern const char kNotificationStatusHistogram[];
enum class SendTabToSelfNotification {
  // The user opened a tab from a notification.
  kOpened = 0,
  // The user closed a notification.
  kDismissed = 1,
  // A notification was shown from a remotely added entry.
  kShown = 2,
  // 3 is once |kDismissedRemotely| and has been obsoleted.
  // Numeric values should skip 3.
  // Update kMaxValue when new enums are added.
  kMaxValue = kShown,
};

void RecordNotificationHistogram(SendTabToSelfNotification status);

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_METRICS_H_