// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKTOP_TO_MOBILE_PROMOS_PROMOS_TYPES_H_
#define COMPONENTS_DESKTOP_TO_MOBILE_PROMOS_PROMOS_TYPES_H_

namespace desktop_to_mobile_promos {

// Promo type (eature being highlighted) for the desktop-to-mobile promos.
enum class PromoType {
  kPassword,
  kAddress,
  kPayment,
  kEnhancedBrowsing,
  kLens
};

// Bubble type for the desktop-to-mobile promos. A promo bubble can either
// feature a QRCode or be a Reminder type.
enum class BubbleType { kQRCode, kReminder, kReminderConfirmation };
}  // namespace desktop_to_mobile_promos

#endif  // COMPONENTS_DESKTOP_TO_MOBILE_PROMOS_PROMOS_TYPES_H_
