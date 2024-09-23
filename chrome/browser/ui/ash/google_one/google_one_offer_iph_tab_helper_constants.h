// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_GOOGLE_ONE_GOOGLE_ONE_OFFER_IPH_TAB_HELPER_CONSTANTS_H_
#define CHROME_BROWSER_UI_ASH_GOOGLE_ONE_GOOGLE_ONE_OFFER_IPH_TAB_HELPER_CONSTANTS_H_

// Fallback texts are used if params are not provided, e.g. IPH demo mode. We
// won't expect that those fallback texts are used in prod.
constexpr char kFallbackNotificationDisplaySource[] = "ChromeOS Perks";
constexpr char kFallbackNotificationTitle[] = "Get 100 GB of cloud storage";
constexpr char kFallbackNotificationMessage[] =
    "Keep your files & photos backed up with 12 months of Google One at no "
    "charge";
constexpr char kFallbackGetPerkButtonTitle[] = "Claim now";

static constexpr char kIPHGoogleOneOfferNotificationId[] =
    "iph-google-one-offer-notification-id";
static constexpr char kIPHGoogleOneOfferNotifierId[] =
    "iph-google-one-offer-notifier-id";

// Notification constants
static constexpr int kGetPerkButtonIndex = 0;
static constexpr char kGoogleOneOfferUrl[] =
    "https://www.google.com/chromebook/perks/?id=google.one.2019";

// IPH events
static constexpr char kIPHGoogleOneOfferNotificationDismissEventName[] =
    "google_one_offer_iph_notification_dismiss";
static constexpr char kIPHGoogleOneOfferNotificationGetPerkEventName[] =
    "google_one_offer_iph_notification_get_perk";

// IPH Feature Param names
constexpr char kNotificationDisplaySourceParamName[] =
    "x_google-one-offer-notification-display-source";
constexpr char kNotificationTitleParamName[] =
    "x_google-one-offer-notification-title";
constexpr char kNotificationMessageParamName[] =
    "x_google-one-offer-notification-message";
constexpr char kGetPerkButtonTitleParamName[] =
    "x_google-one-offer-get-perk-title";

#endif  // CHROME_BROWSER_UI_ASH_GOOGLE_ONE_GOOGLE_ONE_OFFER_IPH_TAB_HELPER_CONSTANTS_H_
