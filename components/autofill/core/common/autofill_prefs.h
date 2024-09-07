// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_PREFS_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_PREFS_H_

#include "build/build_config.h"
#include "google_apis/gaia/core_account_id.h"

class PrefRegistrySimple;
class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace autofill::prefs {

// Alphabetical list of preference names specific to the Autofill
// component. Keep alphabetized, and document each in the .cc file.
// Do not get/set the value of this pref directly. Use provided getter/setter.

// String serving as a seed for ablation studies.
inline constexpr std::string_view kAutofillAblationSeedPref =
    "autofill.ablation_seed";
// Boolean that is true if Autofill is enabled and allowed to save credit card
// data.
inline constexpr char kAutofillCreditCardEnabled[] =
    "autofill.credit_card_enabled";
// Boolean that is true if FIDO Authentication is enabled for card unmasking.
inline constexpr char kAutofillCreditCardFidoAuthEnabled[] =
    "autofill.credit_card_fido_auth_enabled";
#if BUILDFLAG(IS_ANDROID)
// Boolean that is true if Autofill is enabled and allowed to save data.
inline constexpr char kAutofillCreditCardFidoAuthOfferCheckboxState[] =
    "autofill.credit_card_fido_auth_offer_checkbox_state";
#endif  // BUILDFLAG(IS_ANDROID)
// Please use kAutofillCreditCardEnabled and kAutofillProfileEnabled instead.
inline constexpr char kAutofillEnabledDeprecated[] = "autofill.enabled";
// Boolean that is true if a form with an IBAN field has ever been submitted, or
// an IBAN has ever been saved via Chrome payments settings page. This helps to
// enable IBAN functionality for those users who are not in a country where IBAN
// is generally available but have used IBAN already.
inline constexpr char kAutofillHasSeenIban[] = "autofill.has_seen_iban";
// Boolean that is true if Autofill is enabled and allowed to save IBAN data.
inline constexpr char kAutofillIbanEnabled[] = "autofill.iban_enabled";
// Integer that is set to the last version where the profile deduping routine
// was run. This routine will be run once per version.
inline constexpr char kAutofillLastVersionDeduped[] =
    "autofill.last_version_deduped";
// To simplify the rollout of AutofillSilentlyRemoveQuasiDuplicates,
// deduplication can be run a second time per milestone for users enrolled in
// the experiment. This pref tracks whether deduplication was run a second time.
// TODO(crbug.com/325450676): Remove after the rollout finished.
inline constexpr char kAutofillRanQuasiDuplicateExtraDeduplication[] =
    "autofill.ran_quasi_duplicate_extra_deduplication";
// Integer that is set to the last version where disused addresses were
// deleted. This deletion will be run once per version.
inline constexpr char kAutofillLastVersionDisusedAddressesDeleted[] =
    "autofill.last_version_disused_addresses_deleted";
// Integer that is set to the last version where disused credit cards were
// deleted. This deletion will be run once per version.
inline constexpr char kAutofillLastVersionDisusedCreditCardsDeleted[] =
    "autofill.last_version_disused_credit_cards_deleted";
// Boolean that is true if the orphan rows in the autofill table were removed.
inline constexpr char kAutofillOrphanRowsRemoved[] =
    "autofill.orphan_rows_removed";
// Boolean that is true, when users can save their CVCs.
inline constexpr char kAutofillPaymentCvcStorage[] =
    "autofill.payment_cvc_storage";
// Boolean that is true when users can see the card benefits with the card.
inline constexpr char kAutofillPaymentCardBenefits[] =
    "autofill.payment_card_benefits";
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
// Boolean that controls improved autofill filling predictions. When enabled,
// the autofill functionality is enhanced with adopting user data to
// the form being filled in, which is triggered by the user via an extra
// autofill suggestion.
inline constexpr char kAutofillPredictionImprovementsEnabled[] =
    "autofill.prediction_improvements.enabled";
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)
// Boolean that is true if Autofill is enabled and allowed to save profile data.
// Do not get/set the value of this pref directly. Use provided getter/setter.
inline constexpr char kAutofillProfileEnabled[] = "autofill.profile_enabled";
// The opt-ins for Sync Transport features for each client.
inline constexpr char kAutofillSyncTransportOptIn[] =
    "autofill.sync_transport_opt_ins";
// The file path where the autofill states data is downloaded to.
inline constexpr char kAutofillStatesDataDir[] = "autofill.states_data_dir";
// The (randomly inititialied) seed value to use when encoding form/field
// metadata for randomized uploads. The value of this pref is a string.
inline constexpr char kAutofillUploadEncodingSeed[] =
    "autofill.upload_encoding_seed";
// Dictionary pref used to track which form signature vote uploads have been
// performed. Each entry in the dictionary maps a form signature (reduced
// via a 10-bit modulus) to an integer bit-field where each bit denotes whether
// or not a given vote upload event has occurred.
inline constexpr char kAutofillVoteUploadEvents[] = "autofill.upload_events";
// Dictionary pref used to track which form signature metadata uploads have been
// performed. Each entry in the dictionary maps a form signature (reduced
// via a 10-bit modulus) to an integer flag that denotes whether or not a given
// metadata upload event has occurred.
// Throttling is done for both Autofill and Password Manager metadata uploads.
inline constexpr char kAutofillMetadataUploadEvents[] =
    "autofill.metadata_upload_events";
// The timestamp (seconds since the Epoch UTC) for when the the upload event
// prefs was last reset.
inline constexpr char kAutofillUploadEventsLastResetTimestamp[] =
    "autofill.upload_events_last_reset_timestamp";
// Integer that is set to the last major version where the Autocomplete
// retention policy was run.
inline constexpr char kAutocompleteLastVersionRetentionPolicy[] =
    "autocomplete.retention_policy_last_version";
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_IOS)
// Boolean that is set when payment methods mandatory re-auth is enabled by the
// user.
inline constexpr char kAutofillPaymentMethodsMandatoryReauth[] =
    "autofill.payment_methods_mandatory_reauth";
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_IOS)
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
// Integer that is incremented when the mandatory re-auth promo is shown. If
// this is less than `kMaxValueForMandatoryReauthPromoShownCounter`, that
// implies that the user has not yet decided whether or not to turn on the
// payments mandatory re-auth feature.
inline constexpr char
    kAutofillPaymentMethodsMandatoryReauthPromoShownCounter[] =
        "autofill.payment_methods_mandatory_reauth_promo_counter";
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_ANDROID)
// Boolean that is true iff Chrome only provdides a virtual view structure that
// Android Autofill providers can use for filling. This pref is profile bound
// since each profile may have a preference for filling. It is not syncable as
// the setup on each device requires steps outside the browser. Enabling this
// pref on a device without a proper provider may yield a surprising absence of
// filling.
inline constexpr char kAutofillUsingVirtualViewStructure[] =
    "autofill.using_virtual_view_structure";
// Boolean set by the `ThirdPartyPasswordManagersAllowed` policy. Defaults to
// true which allows users to set the `kAutofillUsingVirtualViewStructure` pref.
// If set to false, user can only use the built-in password manager.
inline constexpr char kAutofillThirdPartyPasswordManagersAllowed[] =
    "autofill.third_party_password_managers_allowed";
inline constexpr char kFacilitatedPaymentsPix[] = "facilitated_payments.pix";
#endif  // BUILDFLAG(IS_ANDROID)

// The maximum value for the
// `kAutofillPaymentMethodsMandatoryReauthPromoShownCounter` pref. If this
// value is reached, we should not show a mandatory re-auth promo.
const int kMaxValueForMandatoryReauthPromoShownCounter = 2;

namespace sync_transport_opt_in {
enum Flags {
  kWallet = 1 << 0,
};
}  // namespace sync_transport_opt_in

// Registers Autofill prefs.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

// Migrates deprecated Autofill prefs values.
void MigrateDeprecatedAutofillPrefs(PrefService* prefs);

bool IsAutocompleteEnabled(const PrefService* prefs);

bool IsCreditCardFIDOAuthEnabled(PrefService* prefs);

void SetCreditCardFIDOAuthEnabled(PrefService* prefs, bool enabled);

bool IsAutofillPaymentMethodsEnabled(const PrefService* prefs);

void SetAutofillPaymentMethodsEnabled(PrefService* prefs, bool enabled);

bool HasSeenIban(const PrefService* prefs);

void SetAutofillHasSeenIban(PrefService* prefs);

bool IsAutofillManaged(const PrefService* prefs);

bool IsAutofillProfileManaged(const PrefService* prefs);

bool IsAutofillCreditCardManaged(const PrefService* prefs);

bool IsAutofillProfileEnabled(const PrefService* prefs);

void SetAutofillProfileEnabled(PrefService* prefs, bool enabled);

bool IsPaymentMethodsMandatoryReauthEnabled(const PrefService* prefs);

// Returns true if the user has ever made an explicit decision for
// this pref. Note that this function returns whether the user has set the pref,
// not the value of the pref itself.
bool IsPaymentMethodsMandatoryReauthSetExplicitly(const PrefService* prefs);

void SetPaymentMethodsMandatoryReauthEnabled(PrefService* prefs, bool enabled);

bool IsPaymentMethodsMandatoryReauthPromoShownCounterBelowMaxCap(
    const PrefService* prefs);

void IncrementPaymentMethodsMandatoryReauthPromoShownCounter(
    PrefService* prefs);

bool IsPaymentCvcStorageEnabled(const PrefService* prefs);

void SetPaymentCvcStorage(PrefService* prefs, bool value);

bool IsPaymentCardBenefitsEnabled(const PrefService* prefs);

void SetPaymentCardBenefits(PrefService* prefs, bool value);

void SetUserOptedInWalletSyncTransport(PrefService* prefs,
                                       const CoreAccountId& account_id,
                                       bool opted_in);

bool IsUserOptedInWalletSyncTransport(const PrefService* prefs,
                                      const CoreAccountId& account_id);

void ClearSyncTransportOptIns(PrefService* prefs);

bool UsesVirtualViewStructureForAutofill(const PrefService* prefs);

void SetFacilitatedPaymentsPix(PrefService* prefs, bool value);

bool IsFacilitatedPaymentsPixEnabled(const PrefService* prefs);

}  // namespace autofill::prefs

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_PREFS_H_
