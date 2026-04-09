// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_METRICS_UTIL_H_
#define COMPONENTS_SEND_TAB_TO_SELF_METRICS_UTIL_H_

#include "base/time/time.h"

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

// Status of the auto-open attempt for a received STTS tab.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.chrome.browser.share.send_tab_to_self)
// LINT.IfChange(AutoOpenOutcome)
enum class AutoOpenOutcome {
  kSuccess = 0,
  kPending = 1,
  kOpenedPending = 2,
  kMaxValue = kOpenedPending,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sharing/enums.xml:SendTabToSelfAutoOpenOutcome)

// Records the outcome of an auto-open attempt.
void RecordAutoOpenOutcome(AutoOpenOutcome outcome);

// Status of scroll position generation when sending a tab.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.chrome.browser.share.send_tab_to_self)
// LINT.IfChange(ScrollPositionGenerationOutcome)
enum class ScrollPositionGenerationOutcome {
  kSuccess = 0,
  kBrowserTimeout = 1,
  kMainFrameChanged = 2,
  kMainFrameUnavailable = 3,
  kEmptySelector = 4,
  kLinkGenerationError = 5,
  kInvalidSelector = 6,
  kRendererTimeout = 7,
  kMaxValue = kRendererTimeout,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sharing/enums.xml:SendTabToSelfScrollPositionGenerationOutcome)

// Records the time taken to generate the scroll position when sending a tab.
void RecordScrollPositionGenerationTime(base::TimeDelta time);

// Records the outcome of scroll position generation when sending a tab.
void RecordScrollPositionGenerationOutcome(
    ScrollPositionGenerationOutcome outcome);

// Records the length of the generated scroll position selector.
void RecordScrollPositionSelectorLength(size_t length);

// Records whether an opened STTS notification contained a scroll position.
void RecordHasScrollPositionOnOpened(bool has_scroll_position);

// Records the size of the PageContext proto when sending a tab, before
// truncation.
void RecordPageContextSize(size_t size);

// Records the volume of scroll interaction after an STTS tab is opened.
// `with_restoration` is true if scroll restoration was attempted.
void RecordScrollVolume(float volume, bool with_restoration);

// Records the time from when a tab was shared (on the sending device) to when
// it was first received by the target device's bridge. Note: this involves
// clocks on two different devices so the value may be skewed.
void RecordTimeSentToReceived(base::TimeDelta delay);

// Records the time from when a tab was shared (on the sending device) to when
// it was opened by the user on the target device. Note: this involves clocks
// on two different devices so the value may be skewed.
void RecordTimeSentToOpened(base::TimeDelta delay);

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_METRICS_UTIL_H_
