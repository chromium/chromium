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

// If enabled, when a server card is unmasked, its info will be cached until
// page navigation to simplify consecutive fills on the same page.
const base::Feature kAutofillCacheServerCardInfo{
    "AutofillCacheServerCardInfo", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAutofillCreditCardAblationExperiment{
    "AutofillCreditCardAblationExperiment", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the use of platform authenticators through WebAuthn to retrieve
// credit cards from Google payments.
const base::Feature kAutofillCreditCardAuthentication{
  "AutofillCreditCardAuthentication",
#if defined(OS_WIN) || defined(OS_MAC)
      // Better Auth project is fully launched on Win/Mac.
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// When enabled, if credit card upload succeeded, the avatar icon will show a
// highlight otherwise, the credit card icon image will be updated and if user
// clicks on the icon, a save card failure bubble will pop up.
const base::Feature kAutofillCreditCardUploadFeedback{
    "AutofillCreditCardUploadFeedback", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, the credit card nicknames will be manageable. They can be
// modified locally.
const base::Feature kAutofillEnableCardNicknameManagement{
    "AutofillEnableCardNicknameManagement", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, shows the Google Pay logo on CVC prompt on Android.
const base::Feature kAutofillDownstreamCvcPromptUseGooglePayLogo{
    "AutofillDownstreamCvcPromptUseGooglePayLogo",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, the credit card nicknames will be manageable. They can be
// uploaded to Payments.
const base::Feature kAutofillEnableCardNicknameUpstream{
    "AutofillEnableCardNicknameUpstream", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, autofill payments bubbles' result will be recorded as either
// 'accepted', 'cancelled', 'closed', 'not interacted' or 'lost focus'.
const base::Feature kAutofillEnableFixedPaymentsBubbleLogging{
    "AutofillEnableFixedPaymentsBubbleLogging",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether we show a Google-issued card in the suggestions list.
const base::Feature kAutofillEnableGoogleIssuedCard{
    "AutofillEnableGoogleIssuedCard", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, offer data will be retrieved during downstream and shown in
// the dropdown list.
const base::Feature kAutofillEnableOffersInDownstream{
    "kAutofillEnableOffersInDownstream", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, all payments related bubbles will not be dismissed upon page
// navigation.
const base::Feature kAutofillEnableStickyPaymentsBubble{
    "AutofillEnableStickyPaymentsBubble", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, Autofill data related icons will be shown in the status
// chip in toolbar along with the avatar toolbar button.
const base::Feature kAutofillEnableToolbarStatusChip{
    "AutofillEnableToolbarStatusChip", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, the option of using cloud token virtual card will be offered
// when all requirements are met.
const base::Feature kAutofillEnableVirtualCard{
    "AutofillEnableVirtualCard", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, the Save Card infobar will be dismissed by a user initiated
// navigation other than one caused by submitted form.
const base::Feature kAutofillSaveCardDismissOnNavigation{
    "AutofillSaveCardDismissOnNavigation", base::FEATURE_ENABLED_BY_DEFAULT};

// When enabled, the Save Card infobar supports editing before submitting.
const base::Feature kAutofillSaveCardInfobarEditSupport{
    "AutofillSaveCardInfobarEditSupport", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls offering credit card upload to Google Payments. Cannot ever be
// ENABLED_BY_DEFAULT because the feature state depends on the user's country.
// There are countries we simply can't turn this on for, and they change over
// time, so it's important that we can flip a switch and be done instead of
// having old versions of Chrome forever do the wrong thing. Enabling it by
// default would mean that any first-run client without a Finch config won't get
// the overriding command to NOT turn it on, which becomes an issue.
const base::Feature kAutofillUpstream{"AutofillUpstream",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillUpstreamAllowAllEmailDomains{
    "AutofillUpstreamAllowAllEmailDomains", base::FEATURE_DISABLED_BY_DEFAULT};

bool ShouldShowImprovedUserConsentForCreditCardSave() {
#if defined(OS_WIN) || defined(OS_APPLE) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  // The new user consent UI is fully launched on MacOS, Windows and Linux.
  return true;
#else
  // Chrome OS does not have the new UI.
  return false;
#endif
}

}  // namespace features
}  // namespace autofill
