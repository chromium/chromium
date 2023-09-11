// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_OFFER_NOTIFICATION_OPTIONS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_OFFER_NOTIFICATION_OPTIONS_H_

namespace autofill {

// Contains the necessary information to determine how the offer notification UI
// should show.
struct OfferNotificationOptions {
  // Indicates whether this notification has been shown since profile start-up.
  bool notification_has_been_shown = false;
  // Indicates whether the notification will automatically expand upon being
  // shown.
  bool expand_notification_icon = false;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_OFFER_NOTIFICATION_OPTIONS_H_
