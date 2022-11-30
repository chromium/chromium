// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_METRICS_UTIL_H_
#define COMPONENTS_SEND_TAB_TO_SELF_METRICS_UTIL_H_

namespace send_tab_to_self {

enum class ShareEntryPoint {
  kContentMenu,
  kLinkMenu,
  kOmniboxIcon,
  kOmniboxMenu,
  kShareMenu,
  kShareSheet,
  kTabMenu,
};

// State of the send tab to self option in the UI.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// For historical reasons, this maps to SendTabToSelfClickResult in enums.xml.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.chrome.browser.share.send_tab_to_self)
enum class SendingEvent {
  // kShowItem = 0,
  kClickItem = 1,
  kShowDeviceList = 2,
  kShowNoTargetDeviceMessage = 3,
  kShowSigninPromo = 4,
  kMaxValue = kShowSigninPromo,
};

void RecordSendingEvent(ShareEntryPoint entry_point, SendingEvent event);

// Records when a received STTS notification is shown.
void RecordNotificationShown();

// Records when a received STTS notification is dismissed.
void RecordNotificationDismissed();

// Records when a received STTS notification is opened.
void RecordNotificationOpened();

// Records when a received STTS notification is shown and times out.
void RecordNotificationTimedOut();

// Records when a received STTS notification is dismissed for an unknown reason.
void RecordNotificationDismissReasonUnknown();

// Records when a received STTS notification is throttled from being sent.
void RecordNotificationThrottled();

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_METRICS_UTIL_H_
