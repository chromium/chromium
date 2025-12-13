// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKTOP_TO_MOBILE_PROMOS_DESKTOP_TO_MOBILE_PROMOS_METRICS_H_
#define COMPONENTS_DESKTOP_TO_MOBILE_PROMOS_DESKTOP_TO_MOBILE_PROMOS_METRICS_H_

#include "components/desktop_to_mobile_promos/promos_types.h"

namespace desktop_to_mobile_promos {

// Enum for the IOS.Desktop.{PromoType}.BubbleView.Created histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DesktopPromoBubbleType {
  kReminder = 0,
  kQRCode = 1,
  kReminderConfirmation = 2,
  kMaxValue = kReminderConfirmation,
};

// Enum for the IOS.Desktop.{PromoType}.{BubbleType}.Action histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DesktopPromoActionType {
  kAccept = 0,
  kCancel = 1,
  kDismiss = 2,
  kMaxValue = kDismiss,
};

// Logs the creation of the promo bubble.
void LogDesktopPromoBubbleCreated(PromoType promo_type, BubbleType bubble_type);

// Logs the action taken on the promo bubble.
void LogDesktopPromoAction(PromoType promo_type,
                           BubbleType bubble_type,
                           DesktopPromoActionType action);

}  // namespace desktop_to_mobile_promos

#endif  // COMPONENTS_DESKTOP_TO_MOBILE_PROMOS_DESKTOP_TO_MOBILE_PROMOS_METRICS_H_
