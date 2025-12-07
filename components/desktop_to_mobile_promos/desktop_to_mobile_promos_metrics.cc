// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desktop_to_mobile_promos/desktop_to_mobile_promos_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"

namespace desktop_to_mobile_promos {

std::string PromoTypeToString(PromoType type) {
  switch (type) {
    case PromoType::kPassword:
      return "Password";
    case PromoType::kEnhancedBrowsing:
      return "EnhancedBrowsing";
    case PromoType::kLens:
      return "Lens";
    case PromoType::kAddress:
      return "Address";
    case PromoType::kPayment:
      return "Payment";
  }
}

std::string BubbleTypeToString(BubbleType type) {
  switch (type) {
    case BubbleType::kQRCode:
      return "QRCode";
    case BubbleType::kReminder:
      return "Reminder";
    case BubbleType::kReminderConfirmation:
      return "ReminderConfirmation";
  }
}

DesktopPromoBubbleType BubbleTypeToDesktopPromoBubbleType(BubbleType type) {
  switch (type) {
    case BubbleType::kReminder:
      return DesktopPromoBubbleType::kReminder;
    case BubbleType::kQRCode:
      return DesktopPromoBubbleType::kQRCode;
    case BubbleType::kReminderConfirmation:
      return DesktopPromoBubbleType::kReminderConfirmation;
  }
}

void LogDesktopPromoBubbleCreated(PromoType promo_type,
                                  BubbleType bubble_type) {
  base::UmaHistogramEnumeration(
      base::StrCat({"UserEducation.DesktopToIOSPromo.",
                    PromoTypeToString(promo_type), ".BubbleView.Created"}),
      BubbleTypeToDesktopPromoBubbleType(bubble_type));
}

void LogDesktopPromoAction(PromoType promo_type,
                           BubbleType bubble_type,
                           DesktopPromoActionType action) {
  base::UmaHistogramEnumeration(
      base::StrCat({"UserEducation.DesktopToIOSPromo.",
                    PromoTypeToString(promo_type), ".",
                    BubbleTypeToString(bubble_type), ".Action"}),
      action);
}

}  // namespace desktop_to_mobile_promos
