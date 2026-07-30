// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_METRICS_UTIL_H_
#define COMPONENTS_SEND_TAB_TO_SELF_METRICS_UTIL_H_

#include <stddef.h>

#include <optional>

#include "base/time/time.h"
#include "components/send_tab_to_self/entry_point_display_reason.h"
#include "components/sync_device_info/device_info.h"

namespace send_tab_to_self {

enum class SendTabToSelfResult;

// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.chrome.browser.share.send_tab_to_self)
// LINT.IfChange(SendTabToSelfShareEntryPoint)
enum class ShareEntryPoint {
  // The context menu on a WebContents.
  kContentMenu = 0,
  // The context menu on a link.
  kLinkMenu = 1,
  // The icon in the toolbar, next to the Omnibox.
  kToolbarIcon = 2,
  // The context menu on the Omnibox.
  kOmniboxMenu = 3,
  // The Share menu in the 3dot menu.
  kShareMenu = 4,
  // The OS-level Share Sheet.
  kShareSheet = 5,
  // The context menu on a tab (in the tab strip or tab switcher).
  kTabMenu = 6,
  // A physical gesture.
  kGesture = 7,
  // A DirectShare target on the OS-level Share Sheet.
  kShareSheetDirectShare = 8,
  kMaxValue = kShareSheetDirectShare,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sharing/enums.xml:SendTabToSelfShareEntryPoint)

// Records the entry point from which the Send Tab to Self feature was invoked.
void RecordEntryPointInvoked(ShareEntryPoint entry_point);

// Records the entry point from which the Send Tab to Self feature successfully
// sent a tab.
void RecordEntryPointSent(ShareEntryPoint entry_point);

// Records the result of attempting to send a tab.
void RecordSendResult(SendTabToSelfResult result);

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
  // Tab(s) received but could not be opened immediately. This happens when:
  // - Desktop: No active browser window is available.
  // - Android: The app is in the background (a system notification is shown).
  // - iOS: The app is in the background or the active tab is not visible (e.g.,
  //   user is in the Tab Grid).
  kUnopenedImmediately = 0,

  // Tab(s) opened immediately upon receipt (browser was active).
  kTabOpenedInForeground = 1,  // Desktop only (single tab).
  // Desktop (subsequent tabs) & Mobile (all tabs).
  kTabsOpenedImmediatelyInBackground = 2,

  // Tab(s) opened delayed (browser was inactive/backgrounded/closed when
  // received). Opened when window became available (Desktop) or app
  // foregrounded (Mobile).
  kTabsOpenedInBackgroundUponActivation = 3,

  // Tab opened via explicit user interaction, i.e. Android/iOS notification
  // click.
  kTabOpenedViaNotification = 4,

  // Tab opened in native app immediately (browser was active).
  kOpenedInNativeAppImmediately = 5,

  // Tab opened in native app delayed (browser was inactive/backgrounded/closed
  // when received).
  kOpenedInNativeAppUponActivation = 6,

  // Tab(s) received but could not be opened upon app activation. This happens
  // when a previous received entry triggered a switch to another app.
  kUnopenedUponActivation = 7,

  kMaxValue = kUnopenedUponActivation,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sharing/enums.xml:SendTabToSelfAutoOpenOutcome)

// Records the outcome of an auto-open attempt.
void RecordAutoOpenOutcome(AutoOpenOutcome outcome);

// Entry point from which a received Send Tab to Self tab was activated.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.chrome.browser.share.send_tab_to_self)
// LINT.IfChange(SendTabToSelfShareActivatedEntryPoint)
enum class ShareActivatedEntryPoint {
  // Automatically activated in the foreground (no user interaction required).
  kAutoOpened = 0,
  // Activated by clicking the action button on the desktop toast (for tabs
  // auto-opened in the background).
  kDesktopToast = 1,
  // Activated via the "Open in New Tab" button in the desktop toolbar promo
  // bubble (when auto-open is disabled).
  kDesktopToolbarBubble = 2,
  // Activated by tapping the system notification on mobile.
  kMobileNotification = 3,
  // Activated manually by selecting the tab in the tab strip on desktop/tablet,
  // or from the tab switcher on mobile (for tabs auto-opened in the
  // background).
  // Note: On desktop, this is only recorded if the tab is activated before
  // Chrome is shut down or restarted.
  kTabStrip = 4,
  // Activated from the ChromeOS Birch suggestion chip.
  kChromeOSBirch = 5,
  // The tab was closed or the browser was shut down/restarted before the tab
  // was activated.
  kTabOrBrowserClosedWithoutActivation = 6,
  // The entry expired in the database before it was activated.
  kSTTSEntryExpiredWithoutActivation = 7,
  kMaxValue = kSTTSEntryExpiredWithoutActivation,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sharing/enums.xml:SendTabToSelfShareActivatedEntryPoint)

// Records the entry point from which a received tab was opened.
void RecordActivatedEntryPoint(ShareActivatedEntryPoint entry_point);

// Outcome of matching a received form field to a field on the page.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SendTabToSelfFormFieldMatchOutcome)
enum class FormFieldMatchOutcome {
  kMatchedByIdNameAndType = 0,
  kMatchedBySignature = 1,
  kMatchedByExactTypeSet = 2,
  kNoMatch = 3,
  kMaxValue = kNoMatch,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sharing/enums.xml:SendTabToSelfFormFieldMatchOutcome)

// Records the outcome of matching a received form field.
void RecordFormFieldMatchOutcome(FormFieldMatchOutcome outcome, int count = 1);

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

// Records the time from when a tab was opened to when it was activated on the
// target device.
void RecordTimeOpenedToActivated(base::TimeDelta delay);

// Records the time from when a tab was shared (on the sending device) to when
// it was activated by the user on the target device. Note: this involves
// clocks on two different devices so the value may be skewed.
void RecordTimeSentToActivated(base::TimeDelta delay);

// Form factor combinations for sending/receiving devices.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SendTabToSelfFormFactorCombination)
enum class SendTabToSelfFormFactorCombination {
  kDesktopToDesktop = 0,
  kDesktopToPhone = 1,
  kDesktopToTablet = 2,
  kDesktopToUnknown = 3,
  kPhoneToDesktop = 4,
  kPhoneToPhone = 5,
  kPhoneToTablet = 6,
  kPhoneToUnknown = 7,
  kTabletToDesktop = 8,
  kTabletToPhone = 9,
  kTabletToTablet = 10,
  kTabletToUnknown = 11,
  kUnknownToDesktop = 12,
  kUnknownToPhone = 13,
  kUnknownToTablet = 14,
  kUnknownToUnknown = 15,
  kMaxValue = kUnknownToUnknown,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sharing/enums.xml:SendTabToSelfFormFactorCombination)

void RecordDeviceFormFactorCombination(
    syncer::DeviceInfo::FormFactor sender_form_factor,
    syncer::DeviceInfo::FormFactor target_form_factor);

// Keep in sync with SendTabToSelfDeviceCount in enums.xml.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SendTabToSelfDeviceCount)
enum class SendTabToSelfDeviceCount {
  kNoTargetDevicesBecauseSignedOut = 0,
  kZeroDevices = 1,
  kOneDevice = 2,
  kTwoDevices = 3,
  kThreeDevices = 4,
  kFourDevices = 5,
  kFiveDevices = 6,
  kMoreThanFiveDevices = 7,
  kNoTargetDevicesBecauseSigninPending = 8,
  kMaxValue = kNoTargetDevicesBecauseSigninPending,
};
// LINT.ThenChange(/tools/metrics/histograms/enums.xml:SendTabToSelfDeviceCount)

void RecordTargetDeviceCount(ShareEntryPoint entry_point,
                             EntryPointDisplayReason display_reason,
                             size_t device_count);

// Records whether the local device name is available when sending an STTS
// entry.
void RecordIsLocalDeviceNameAvailableOnSend(bool is_available);

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_METRICS_UTIL_H_
