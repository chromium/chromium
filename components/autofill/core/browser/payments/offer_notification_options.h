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
  // This is only used in Android.
  // TODO(crbug.com/40931835): Revisit if we can consolidate
  // notification_has_been_shown and show_notification_automatically.
  bool notification_has_been_shown = false;
  // Indicates whether the notification will automatically expand upon being
  // shown.
  bool expand_notification_icon = false;
  // Indicates whether the notification bubble should be shown automatically
  // when the user navigates to the qualified page.
  bool show_notification_automatically = false;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_OFFER_NOTIFICATION_OPTIONS_H_
