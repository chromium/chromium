// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/metrics_util.h"

#include <cmath>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"

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

SendTabToSelfFormFactorCombination GetFormFactorCombination(
    syncer::DeviceInfo::FormFactor sender_form_factor,
    syncer::DeviceInfo::FormFactor target_form_factor) {
  switch (sender_form_factor) {
    case syncer::DeviceInfo::FormFactor::kDesktop:
      switch (target_form_factor) {
        case syncer::DeviceInfo::FormFactor::kDesktop:
          return SendTabToSelfFormFactorCombination::kDesktopToDesktop;
        case syncer::DeviceInfo::FormFactor::kPhone:
          return SendTabToSelfFormFactorCombination::kDesktopToPhone;
        case syncer::DeviceInfo::FormFactor::kTablet:
          return SendTabToSelfFormFactorCombination::kDesktopToTablet;
        case syncer::DeviceInfo::FormFactor::kUnknown:
        case syncer::DeviceInfo::FormFactor::kAutomotive:
        case syncer::DeviceInfo::FormFactor::kWearable:
        case syncer::DeviceInfo::FormFactor::kTv:
          return SendTabToSelfFormFactorCombination::kDesktopToUnknown;
      }
    case syncer::DeviceInfo::FormFactor::kPhone:
      switch (target_form_factor) {
        case syncer::DeviceInfo::FormFactor::kDesktop:
          return SendTabToSelfFormFactorCombination::kPhoneToDesktop;
        case syncer::DeviceInfo::FormFactor::kPhone:
          return SendTabToSelfFormFactorCombination::kPhoneToPhone;
        case syncer::DeviceInfo::FormFactor::kTablet:
          return SendTabToSelfFormFactorCombination::kPhoneToTablet;
        case syncer::DeviceInfo::FormFactor::kUnknown:
        case syncer::DeviceInfo::FormFactor::kAutomotive:
        case syncer::DeviceInfo::FormFactor::kWearable:
        case syncer::DeviceInfo::FormFactor::kTv:
          return SendTabToSelfFormFactorCombination::kPhoneToUnknown;
      }
    case syncer::DeviceInfo::FormFactor::kTablet:
      switch (target_form_factor) {
        case syncer::DeviceInfo::FormFactor::kDesktop:
          return SendTabToSelfFormFactorCombination::kTabletToDesktop;
        case syncer::DeviceInfo::FormFactor::kPhone:
          return SendTabToSelfFormFactorCombination::kTabletToPhone;
        case syncer::DeviceInfo::FormFactor::kTablet:
          return SendTabToSelfFormFactorCombination::kTabletToTablet;
        case syncer::DeviceInfo::FormFactor::kUnknown:
        case syncer::DeviceInfo::FormFactor::kAutomotive:
        case syncer::DeviceInfo::FormFactor::kWearable:
        case syncer::DeviceInfo::FormFactor::kTv:
          return SendTabToSelfFormFactorCombination::kTabletToUnknown;
      }
    case syncer::DeviceInfo::FormFactor::kUnknown:
    case syncer::DeviceInfo::FormFactor::kAutomotive:
    case syncer::DeviceInfo::FormFactor::kWearable:
    case syncer::DeviceInfo::FormFactor::kTv:
      switch (target_form_factor) {
        case syncer::DeviceInfo::FormFactor::kDesktop:
          return SendTabToSelfFormFactorCombination::kUnknownToDesktop;
        case syncer::DeviceInfo::FormFactor::kPhone:
          return SendTabToSelfFormFactorCombination::kUnknownToPhone;
        case syncer::DeviceInfo::FormFactor::kTablet:
          return SendTabToSelfFormFactorCombination::kUnknownToTablet;
        case syncer::DeviceInfo::FormFactor::kUnknown:
        case syncer::DeviceInfo::FormFactor::kAutomotive:
        case syncer::DeviceInfo::FormFactor::kWearable:
        case syncer::DeviceInfo::FormFactor::kTv:
          return SendTabToSelfFormFactorCombination::kUnknownToUnknown;
      }
  }
}

SendTabToSelfDeviceCount GetSendTabToSelfDeviceCount(
    EntryPointDisplayReason reason,
    size_t device_count) {
  switch (reason) {
    case EntryPointDisplayReason::kOfferSignIn:
      return SendTabToSelfDeviceCount::kNoTargetDevicesBecauseSignedOut;
    case EntryPointDisplayReason::kOfferReauth:
      return SendTabToSelfDeviceCount::kNoTargetDevicesBecauseSigninPending;
    case EntryPointDisplayReason::kInformNoTargetDevice:
      return SendTabToSelfDeviceCount::kZeroDevices;
    case EntryPointDisplayReason::kOfferFeature:
      if (device_count == 0) {
        return SendTabToSelfDeviceCount::kZeroDevices;
      } else if (device_count == 1) {
        return SendTabToSelfDeviceCount::kOneDevice;
      } else if (device_count == 2) {
        return SendTabToSelfDeviceCount::kTwoDevices;
      } else if (device_count == 3) {
        return SendTabToSelfDeviceCount::kThreeDevices;
      } else if (device_count == 4) {
        return SendTabToSelfDeviceCount::kFourDevices;
      } else if (device_count == 5) {
        return SendTabToSelfDeviceCount::kFiveDevices;
      } else {
        return SendTabToSelfDeviceCount::kMoreThanFiveDevices;
      }
  }
}

std::string GetEntryPointSuffix(ShareEntryPoint entry_point) {
  switch (entry_point) {
    case ShareEntryPoint::kContentMenu:
      return "ContentMenu";
    case ShareEntryPoint::kLinkMenu:
      return "LinkMenu";
    case ShareEntryPoint::kToolbarIcon:
      return "ToolbarIcon";
    case ShareEntryPoint::kOmniboxMenu:
      return "OmniboxMenu";
    case ShareEntryPoint::kShareMenu:
      return "ShareMenu";
    case ShareEntryPoint::kShareSheet:
      return "ShareSheet";
    case ShareEntryPoint::kTabMenu:
      return "TabMenu";
    case ShareEntryPoint::kGesture:
      return "Gesture";
    case ShareEntryPoint::kShareSheetDirectShare:
      return "ShareSheetDirectShare";
  }
}

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

void RecordAutoOpenOutcome(AutoOpenOutcome outcome) {
  base::UmaHistogramEnumeration("Sharing.SendTabToSelf.AutoOpenOutcome2",
                                outcome);
}

void RecordActivatedEntryPoint(ShareActivatedEntryPoint entry_point) {
  base::UmaHistogramEnumeration("Sharing.SendTabToSelf.ActivatedEntryPoint",
                                entry_point);
}

void RecordFormFieldMatchOutcome(FormFieldMatchOutcome outcome, int count) {
  for (int i = 0; i < count; ++i) {
    base::UmaHistogramEnumeration(
        "Sharing.SendTabToSelf.ReceivedTabFormFieldMatchOutcome", outcome);
  }
}

void RecordScrollPositionGenerationTime(base::TimeDelta time) {
  base::UmaHistogramTimes("Sharing.SendTabToSelf.ScrollPosition.GenerationTime",
                          time);
}

void RecordScrollPositionGenerationOutcome(
    ScrollPositionGenerationOutcome outcome) {
  base::UmaHistogramEnumeration(
      "Sharing.SendTabToSelf.ScrollPosition.GenerationOutcome", outcome);
}

void RecordScrollPositionSelectorLength(size_t length) {
  base::UmaHistogramCounts1000(
      "Sharing.SendTabToSelf.ScrollPosition.SelectorLength", length);
}

void RecordHasScrollPositionOnOpened(bool has_scroll_position) {
  base::UmaHistogramBoolean(
      "Sharing.SendTabToSelf.NotificationClicked.HasScrollPosition",
      has_scroll_position);
}

void RecordPageContextSize(size_t size) {
  base::UmaHistogramCounts10000("Sharing.SendTabToSelf.PageContextSize", size);
}

void RecordScrollVolume(float volume, bool with_restoration) {
  const std::string_view name =
      with_restoration
          ? "Sharing.SendTabToSelf.Scroll.Volume.WithRestoration"
          : "Sharing.SendTabToSelf.Scroll.Volume.WithoutRestoration";
  base::UmaHistogramCounts10000(name, static_cast<int>(std::round(volume)));
}

void RecordTimeSentToReceived(base::TimeDelta delay) {
  base::UmaHistogramCustomTimes("Sharing.SendTabToSelf.TimeSentToReceived",
                                delay, base::Milliseconds(100), base::Days(10),
                                100);
}

void RecordTimeSentToOpened(base::TimeDelta delay) {
  base::UmaHistogramCustomTimes("Sharing.SendTabToSelf.TimeSentToOpened", delay,
                                base::Milliseconds(100), base::Days(10), 100);
}

void RecordTimeOpenedToActivated(base::TimeDelta delay) {
  base::UmaHistogramCustomTimes("Sharing.SendTabToSelf.TimeOpenedToActivated",
                                delay, base::Milliseconds(100), base::Days(10),
                                100);
}

void RecordTimeSentToActivated(base::TimeDelta delay) {
  base::UmaHistogramCustomTimes("Sharing.SendTabToSelf.TimeSentToActivated",
                                delay, base::Milliseconds(100), base::Days(10),
                                100);
}

void RecordDeviceFormFactorCombination(
    syncer::DeviceInfo::FormFactor sender_form_factor,
    syncer::DeviceInfo::FormFactor target_form_factor) {
  base::UmaHistogramEnumeration(
      "Sharing.SendTabToSelf.DeviceFormFactorCombination",
      GetFormFactorCombination(sender_form_factor, target_form_factor));
}

void RecordTargetDeviceCount(ShareEntryPoint entry_point,
                             EntryPointDisplayReason display_reason,
                             size_t device_count) {
  SendTabToSelfDeviceCount device_count_bucket =
      GetSendTabToSelfDeviceCount(display_reason, device_count);
  // Record the general/aggregate histogram.
  base::UmaHistogramEnumeration("Sharing.SendTabToSelf.TargetDeviceCount",
                                device_count_bucket);

  // Record the per-entry-point breakdown histogram.
  base::UmaHistogramEnumeration(
      base::StrCat({"Sharing.SendTabToSelf.TargetDeviceCount.",
                    GetEntryPointSuffix(entry_point)}),
      device_count_bucket);
}

void RecordEntryPointInvoked(ShareEntryPoint entry_point) {
  base::UmaHistogramEnumeration("Sharing.SendTabToSelf.InvokedEntryPoint",
                                entry_point);
}

void RecordEntryPointSent(ShareEntryPoint entry_point) {
  base::UmaHistogramEnumeration("Sharing.SendTabToSelf.SentEntryPoint",
                                entry_point);
}

void RecordSendResult(SendTabToSelfResult result) {
  base::UmaHistogramEnumeration("Sharing.SendTabToSelf.SendResult", result);
}

void RecordIsLocalDeviceNameAvailableOnSend(bool is_available) {
  base::UmaHistogramBoolean(
      "Sharing.SendTabToSelf.IsLocalDeviceNameAvailableOnSend", is_available);
}

}  // namespace send_tab_to_self
