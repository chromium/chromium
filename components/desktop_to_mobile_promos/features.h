// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKTOP_TO_MOBILE_PROMOS_FEATURES_H_
#define COMPONENTS_DESKTOP_TO_MOBILE_PROMOS_FEATURES_H_

#include "base/containers/enum_set.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "components/desktop_to_mobile_promos/promos_types.h"

// Enum to represent promo types of feature kMobilePromoOnDesktop.
// LINT.IfChange(MobilePromoOnDesktopPromoType)
enum class MobilePromoOnDesktopPromoType {
  kMinValue = 0,
  // Indicates that all types of promos are enabled for the device.
  kAllPromos = kMinValue,
  kLensPromo = 1,
  kESBPromo = 2,
  kAutofillPromo = 3,
  kTabGroups = 4,
  kPriceTracking = 5,
  kMaxValue = kPriceTracking,
};
// LINT.ThenChange(/components/sync/protocol/sync_enums.proto:MobilePromoOnDesktopPromoType)

// Represents a set of MobilePromoOnDesktopPromoType values.
// Used primarily to sync the specific types of promos an iOS device is
// eligible to receive to Desktop via DeviceInfo.
using MobilePromoOnDesktopPromoTypeSet =
    base::EnumSet<MobilePromoOnDesktopPromoType,
                  MobilePromoOnDesktopPromoType::kMinValue,
                  MobilePromoOnDesktopPromoType::kMaxValue>;

// Enum to represent the forced promo type of feature
// kMobilePromoOnDesktopForcePromoType.
enum class IOSPromoBubbleForceType {
  kQRCode = 0,
  kReminder = 1,
  kNoOverride = 2,
};

// Enum to represent variations of feature kMobileNTPPromoOnDesktop.
enum class MobileNTPPromoOnDesktopVariation {
  kGeneral = 0,
  kPasswords = 1,
  kAll = 2,
};

// If this feature is enabled, show mobile promo on desktop with a "Remind Me"
// button.
BASE_DECLARE_FEATURE(kMobilePromoOnDesktopWithReminder);
BASE_DECLARE_FEATURE(kMobilePromoOnDesktopWithReminderWave1);

// If this feature is enabled, collect data for the mobile promo on desktop.
BASE_DECLARE_FEATURE(kMobilePromoOnDesktopRecordActiveDays);

// If this feature is enabled, show the QR Code flow for the mobile
// promo on desktop.
BASE_DECLARE_FEATURE(kMobilePromoOnDesktopWithQRCode);
BASE_DECLARE_FEATURE(kMobilePromoOnDesktopWithQRCodeWave1);

// If this feature is enabled, force the iOS promo to be a specific type.
BASE_DECLARE_FEATURE(kMobilePromoOnDesktopForcePromoType);

// If this feature is enabled, show the mobile NTP promo on desktop.
BASE_DECLARE_FEATURE(kMobileNTPPromoOnDesktop);

// Parameter of `kMobilePromoOnDesktop` for promo type.
extern const char kMobilePromoOnDesktopPromoTypeParam[];
// Parameter of `kMobilePromoOnDesktop` for showing the iOS push notification.
extern const char kMobilePromoOnDesktopNotificationParam[];

// Parameter of `kMobilePromoOnDesktopForcePromoType` for the promo type.
extern const char kMobilePromoOnDesktopForcePromoTypeParam[];

// Parameter of `kMobileNTPPromoOnDesktop` for the variation.
extern const char kMobileNTPPromoOnDesktopVariationParam[];

// Returns true if either the `kMobilePromoOnDesktopWithReminder` or
// `kMobilePromoOnDesktopWithQRCode` feature is enabled.
bool MobilePromoOnDesktopEnabled();

// Returns true if the `kMobilePromoOnDesktopRecordActiveDays` feature is
// enabled.
bool IsMobilePromoOnDesktopRecordActiveDaysEnabled();

// Returns whether the given promo type is enabled for feature
// `kMobilePromoOnDesktop` or `kMobilePromoOnDesktopWithQRCode`. If the promo
// type parameter for the feature is not set, this will return true for any
// promo type.
bool MobilePromoOnDesktopTypeEnabled(MobilePromoOnDesktopPromoType type);

// Returns whether the given promo type is enabled for the feature corresponding
// to the provided `bubble_type` (`kMobilePromoOnDesktopWithReminder` for
// Reminder or `kMobilePromoOnDesktopWithQRCode` for QRCode).
bool MobilePromoOnDesktopTypeEnabled(
    MobilePromoOnDesktopPromoType type,
    desktop_to_mobile_promos::BubbleType bubble_type);

// Returns true if feature `kMobilePromoOnDesktopWithReminder` is enabled with a
// push notification arm, false otherwise.
bool IsMobilePromoOnDesktopNotificationsEnabled();

// Returns the forced promo type if `kMobilePromoOnDesktopForcePromoType` is
// enabled, otherwise returns `IOSPromoBubbleType::kNoOverride`.
IOSPromoBubbleForceType GetMobilePromoOnDesktopForcePromoType();

// Returns true if the `kMobileNTPPromoOnDesktop` feature is enabled.
bool IsMobileNTPPromoOnDesktopEnabled();

// Returns whether the given variation is enabled for feature
// `kMobileNTPPromoOnDesktop`.
bool IsMobileNTPPromoOnDesktopVariationEnabled(
    MobileNTPPromoOnDesktopVariation variation);

#endif  // COMPONENTS_DESKTOP_TO_MOBILE_PROMOS_FEATURES_H_
