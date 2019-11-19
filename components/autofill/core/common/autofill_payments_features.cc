// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_payments_features.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace features {

// Features

// Controls whether or not Autofill client will populate form with CPAN and
// dCVV, rather than FPAN.
const base::Feature kAutofillAlwaysReturnCloudTokenizedCard{
    "AutofillAlwaysReturnCloudTokenizedCard",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillCreditCardAblationExperiment{
    "AutofillCreditCardAblationExperiment", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the use of platform authenticators through WebAuthn to retrieve
// credit cards from Google payments.
const base::Feature kAutofillCreditCardAuthentication{
    "AutofillCreditCardAuthentication", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, if credit card upload succeeded, the avatar icon will show a
// highlight otherwise, the credit card icon image will be updated and if user
// clicks on the icon, a save card failure bubble will pop up.
const base::Feature kAutofillCreditCardUploadFeedback{
    "AutofillCreditCardUploadFeedback", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillDoNotMigrateUnsupportedLocalCards{
    "AutofillDoNotMigrateUnsupportedLocalCards",
    base::FEATURE_ENABLED_BY_DEFAULT};

// When enabled, enable local card migration flow for user who has signed in but
// has not turned on sync.
const base::Feature kAutofillEnableLocalCardMigrationForNonSyncUser{
    "AutofillEnableLocalCardMigrationForNonSyncUser",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, Autofill data related icons will be shown in the status
// chip in toolbar along with the avatar toolbar button.
const base::Feature kAutofillEnableToolbarStatusChip{
    "AutofillEnableToolbarStatusChip", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, will remove the option to save unmasked server cards as
// FULL_SERVER_CARDs upon successful unmask.
const base::Feature kAutofillNoLocalSaveOnUnmaskSuccess{
    "AutofillNoLocalSaveOnUnmaskSuccess", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, no local copy of server card will be saved when upload
// succeeds.
const base::Feature kAutofillNoLocalSaveOnUploadSuccess{
    "AutofillNoLocalSaveOnUploadSuccess", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, the Save Card infobar will be dismissed by a user initiated
// navigation other than one caused by submitted form.
const base::Feature kAutofillSaveCardDismissOnNavigation{
    "AutofillSaveCardDismissOnNavigation", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to show updated UI for the card unmask prompt.
const base::Feature kAutofillUpdatedCardUnmaskPromptUi{
    "AutofillUpdatedCardUnmaskPromptUi", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls offering credit card upload to Google Payments. Cannot ever be
// ENABLED_BY_DEFAULT because it's a country-specific whitelist. There are
// countries we simply can't turn this on for, and they change over time, so
// it's important that we can flip a switch and be done instead of having old
// versions of Chrome forever do the wrong thing. Enabling it by default would
// mean that any first-run client without a Finch config won't get the
// overriding command to NOT turn it on, which becomes an issue.
const base::Feature kAutofillUpstream{"AutofillUpstream",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillUpstreamAllowAllEmailDomains{
    "AutofillUpstreamAllowAllEmailDomains", base::FEATURE_DISABLED_BY_DEFAULT};

// For testing purposes; not to be launched.  When enabled, Chrome Upstream
// always requests that the user enters/confirms cardholder name in the
// offer-to-save dialog, regardless of if it was present or if the user is a
// Google Payments customer.  Note that this will override the detected
// cardholder name, if one was found.
const base::Feature kAutofillUpstreamAlwaysRequestCardholderName{
    "AutofillUpstreamAlwaysRequestCardholderName",
    base::FEATURE_DISABLED_BY_DEFAULT};

// For experimental purposes; not to be made available in chrome://flags. When
// enabled and Chrome Upstream requests the cardholder name in the offer-to-save
// dialog, the field will be blank instead of being prefilled with the name from
// the user's Google Account.
const base::Feature kAutofillUpstreamBlankCardholderNameField{
    "AutofillUpstreamBlankCardholderNameField",
    base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, Chrome Upstream can request the user to enter/confirm cardholder
// name in the offer-to-save bubble if it was not detected or was conflicting
// during the checkout flow and the user is NOT a Google Payments customer.
const base::Feature kAutofillUpstreamEditableCardholderName{
  "AutofillUpstreamEditableCardholderName",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

const base::Feature kAutofillUpstreamEditableExpirationDate{
  "AutofillUpstreamEditableExpirationDate",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

bool ShouldShowImprovedUserConsentForCreditCardSave() {
#if defined(OS_WIN) || defined(OS_MACOSX) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  // The new user consent UI is fully launched on MacOS, Windows and Linux.
  return true;
#endif
  // Chrome OS does not have the new UI.
  return false;
}

}  // namespace features
}  // namespace autofill
