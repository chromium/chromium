// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/addresses/account_name_email_store.h"

#include <optional>

#include "base/hash/hash.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "third_party/re2/src/re2/re2.h"

namespace autofill {

namespace {

constexpr std::string_view kSeparator = "|";

// Returns true if `profile` has the same full name and email address as `info`,
// false otherwise.
bool AutofillProfileMatchesAccountInfo(const AutofillProfile& profile,
                                       const AccountInfo& info) {
  return profile.GetRawInfo(NAME_FULL) == base::UTF8ToUTF16(info.full_name) &&
         profile.GetRawInfo(EMAIL_ADDRESS) == base::UTF8ToUTF16(info.email);
}

void RemoveNickname(std::string& name) {
  static base::NoDestructor<std::unique_ptr<const RE2>> nickname_pattern(
      std::make_unique<const RE2>(
          features::kAutofillNameAndEmailProfileNicknameRegex.Get()));
  RE2::GlobalReplace(&name, **nickname_pattern, "");

  name = base::UTF16ToUTF8(base::TrimWhitespace(base::UTF8ToUTF16(name),
                                                base::TrimPositions::TRIM_ALL));
}

}  // namespace

AccountNameEmailStore::AccountNameEmailStore(
    AddressDataManager& address_data_manager,
    signin::IdentityManager& identity_manager,
    syncer::SyncService& sync_service,
    PrefService& pref_service)
    : address_data_manager_(address_data_manager),
      pref_service_(pref_service) {
  identity_manager_observer_.Observe(&identity_manager);
  sync_service_observer_.Observe(&sync_service);

  pref_registrar_.Init(&pref_service_.get());
  pref_registrar_.Add(
      prefs::kAutofillNameAndEmailProfileNotSelectedCounter,
      base::BindRepeating(&AccountNameEmailStore::OnCounterPrefUpdated,
                          base::Unretained(this)));
}

AccountNameEmailStore::~AccountNameEmailStore() = default;

void AccountNameEmailStore::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  if (!identity_manager_observer_.GetSource()) {
    return;
  }
  const std::optional<CoreAccountInfo>& primary_info =
      identity_manager_observer_.GetSource()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);
  if (!primary_info.has_value() ||
      (!primary_info->IsEmpty() && info.gaia != primary_info->gaia)) {
    return;
  }
  MaybeUpdateOrCreateAccountNameEmail();
}

void AccountNameEmailStore::OnIdentityManagerShutdown(
    signin::IdentityManager*) {
  identity_manager_observer_.Reset();
}

void AccountNameEmailStore::OnSyncShutdown(syncer::SyncService*) {
  sync_service_observer_.Reset();
}

void AccountNameEmailStore::OnStateChanged(syncer::SyncService* sync_service) {
  // Only autofill syncing users are eligible for the kAccountNameEmail
  // profile. Having all relevant data loaded is crucial for correct execution.
  // If the user doesn't have the autofill sync toggle enabled, try to remove
  // the kAccountNameEmail profile.
  std::optional<ProfileUpdateBlockReason> reason =
      GetBlockAccountNameEmailUpdateReason();
  if (!reason.has_value()) {
    MaybeUpdateOrCreateAccountNameEmail();
    return;
  }
  switch (reason.value()) {
    case ProfileUpdateBlockReason::kAutofillSyncToggleDisabled:
    case ProfileUpdateBlockReason::kSyncDisabled:
      RemoveAccountNameEmail(/*is_soft_removal=*/true);
      return;
    case ProfileUpdateBlockReason::kUserSignedOut:
      // User signed out and prefs are no longer synced. Clear their local state
      // to prevent them from leaking into a different account. It is important
      // that this happens after PRIORITY_PREFERENCES stopped syncing, because
      // the metadata should be redownloaded during the next sign-in.
      RemoveAccountNameEmail(/*is_soft_removal=*/true);
      pref_service_->ClearPref(prefs::kAutofillNameAndEmailProfileSignature);
      pref_service_->ClearPref(
          prefs::kAutofillNameAndEmailProfileNotSelectedCounter);
      pref_service_->ClearPref(prefs::kAutofillWasNameAndEmailProfileUsed);
      return;
    case ProfileUpdateBlockReason::kDataNotLoaded:
      // Defer call. When data is loaded, `OnStateChanged` will be called again,
      // reattempting to create the profile.
      return;
  }
}

void AccountNameEmailStore::MaybeUpdateOrCreateAccountNameEmail() {
  if (!ShouldUpdateOrCreateAccountNameEmail()) {
    return;
  }

  std::optional<AccountInfo> extended_info =
      identity_manager_observer_.GetSource()->FindExtendedAccountInfo(
          identity_manager_observer_.GetSource()->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSignin));
  if (!extended_info.has_value()) {
    return;
  }

  UpdateOrCreateAccountNameEmail(extended_info.value());
}

#if BUILDFLAG(IS_IOS)
void AccountNameEmailStore::MaybeUpdateOrCreateAccountNameEmail(
    const std::string& account_name,
    const std::string& email) {
  if (!ShouldUpdateOrCreateAccountNameEmail()) {
    return;
  }

  AccountInfo info;
  info.full_name = account_name;
  info.email = email;
  UpdateOrCreateAccountNameEmail(info);
}
#endif

void AccountNameEmailStore::ApplyChange(const AutofillProfileChange& change) {
  if (change.data_model().record_type() !=
      AutofillProfile::RecordType::kAccountNameEmail) {
    return;
  }
  switch (change.type()) {
    case AutofillProfileChange::REMOVE:
      // REMOVE indicates a hard removal, thus the pref needs to be set.
      pref_service_->SetInteger(
          prefs::kAutofillNameAndEmailProfileNotSelectedCounter,
          features::kAutofillNameAndEmailProfileNotSelectedThreshold.Get() + 1);
      return;
    case AutofillProfileChange::UPDATE:
      // Although the kAccountNameEmail profile is read-only from the user POV,
      // it still supports silent updates.
      return;
    case AutofillProfileChange::ADD:
      return;
    case AutofillProfileChange::HIDE_IN_AUTOFILL:
      // The HIDE_IN_AUTOFILL indicates that the kAccountNameEmail profile was
      // soft removed, since `AddressDataManager` already removed it, there is
      // nothing left to do.
      return;
  }
}

void AccountNameEmailStore::RemoveAccountNameEmail(bool is_soft_removal) {
  const std::vector<const AutofillProfile*> account_name_email_profiles =
      address_data_manager_->GetProfilesByRecordType(
          AutofillProfile::RecordType::kAccountNameEmail);
  if (account_name_email_profiles.empty()) {
    return;
  }

  address_data_manager_->RemoveProfile(
      account_name_email_profiles[0]->guid(),
      /*non_permanent_account_profile_removal=*/is_soft_removal);
}

void AccountNameEmailStore::UpdateOrCreateAccountNameEmail(AccountInfo& info) {
  // During signin the `OnExtendedAccountInfoUpdated` method might call this
  // method with an empty `info.full_name` since not all data arrives all at
  // once and `AccountInfo` is updated multiple times. The kAccountNameEmail
  // profile and hash signature require non-empty full_name value.
  if (info.IsEmpty() || info.full_name.empty()) {
    return;
  }
  CHECK(!info.email.empty());

  // The account name can contain nicknames in one of the following styles:
  // 1. John Smith (JJ)
  // 2. John "JJ" Smith
  // where the JJ is the nickname added to the account data (either by the user
  // itself or as a result of the account being managed by a separate app).
  // The nickname shouldn't be added to the AutofillProfile and is removed.
  RemoveNickname(info.full_name);

  const std::string new_hash = HashAccountInfo(info);
  const bool hashes_different =
      new_hash !=
      pref_service_->GetString(prefs::kAutofillNameAndEmailProfileSignature);
  const bool was_hard_removed =
      pref_service_->GetInteger(
          prefs::kAutofillNameAndEmailProfileNotSelectedCounter) >
      features::kAutofillNameAndEmailProfileNotSelectedThreshold.Get();

  if (!hashes_different && was_hard_removed) {
    // User signed out and then signed in, but previously a hard remove had
    // happened, thus no recreation should happen.
    return;
  }

  const std::vector<const AutofillProfile*> account_name_email_profiles =
      address_data_manager_->GetProfilesByRecordType(
          AutofillProfile::RecordType::kAccountNameEmail);
  const bool account_name_email_exists = !account_name_email_profiles.empty();
  if (account_name_email_exists && AutofillProfileMatchesAccountInfo(
                                       *account_name_email_profiles[0], info)) {
    // A valid kAccountNameEmail profile already exists. This function was
    // called as a side effect of the other, unrelated flow.
    return;
  }

  // If the name not available, the `info.full_name` can default to the
  // `info.email`. In this case, don't create the profile.
  if (info.full_name == info.email) {
    // If the account info was updated, make sure any prior profile is removed.
    RemoveAccountNameEmail(/*is_soft_removal=*/false);
    // Ensure that the hash reflects the current values of the account info.
    pref_service_->SetString(prefs::kAutofillNameAndEmailProfileSignature,
                             new_hash);
    return;
  }

  if (account_name_email_exists) {
    RemoveAccountNameEmail(/*is_soft_removal=*/true);
  }

  address_data_manager_->AddProfile(AutofillProfile{info});
  if (hashes_different) {
    pref_service_->SetString(prefs::kAutofillNameAndEmailProfileSignature,
                             new_hash);
    pref_service_->SetInteger(
        prefs::kAutofillNameAndEmailProfileNotSelectedCounter, 0);
  }
}

std::string AccountNameEmailStore::HashAccountInfo(
    const AccountInfo& info) const {
  return base::NumberToString(base::PersistentHash(
      base::StrCat({info.full_name, kSeparator, info.email})));
}

std::optional<AccountNameEmailStore::ProfileUpdateBlockReason>
AccountNameEmailStore::GetBlockAccountNameEmailUpdateReason() {
  if (!sync_service_observer_.GetSource()) {
    return ProfileUpdateBlockReason::kSyncDisabled;
  }

  if (sync_service_observer_.GetSource()->GetTransportState() ==
      syncer::SyncService::TransportState::DISABLED) {
    return ProfileUpdateBlockReason::kUserSignedOut;
  }

  if (!base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos) &&
      !sync_service_observer_.GetSource()->IsSyncFeatureEnabled()) {
    return ProfileUpdateBlockReason::kSyncDisabled;
  }

  if (!sync_service_observer_.GetSource()
           ->GetUserSettings()
           ->GetSelectedTypes()
           .Has(syncer::UserSelectableType::kAutofill)) {
    return ProfileUpdateBlockReason::kAutofillSyncToggleDisabled;
  }

  if (address_data_manager_->IsAwaitingPendingAddressChanges()) {
    return ProfileUpdateBlockReason::kDataNotLoaded;
  }

  switch (sync_service_observer_.GetSource()->GetDownloadStatusFor(
      syncer::DataType::PRIORITY_PREFERENCES)) {
    case syncer::SyncService::DataTypeDownloadStatus::kWaitingForUpdates:
      return ProfileUpdateBlockReason::kDataNotLoaded;
    case syncer::SyncService::DataTypeDownloadStatus::kUpToDate:
    // If the download status is kError, it will likely not become
    // available anytime soon.
    case syncer::SyncService::DataTypeDownloadStatus::kError:
      return std::nullopt;
  }
}

void AccountNameEmailStore::OnCounterPrefUpdated() {
  if (pref_service_->GetInteger(
          prefs::kAutofillNameAndEmailProfileNotSelectedCounter) <=
      features::kAutofillNameAndEmailProfileNotSelectedThreshold.Get()) {
    return;
  }

  RemoveAccountNameEmail(/*is_soft_removal=*/false);
}

bool AccountNameEmailStore::ShouldUpdateOrCreateAccountNameEmail() {
  if (!identity_manager_observer_.GetSource() ||
      GetBlockAccountNameEmailUpdateReason().has_value()) {
    return false;
  }

  // Primary account has to exists for the AccountNameEmail profile to be
  // created.
  return !identity_manager_observer_.GetSource()
              ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
              .IsEmpty();
}

}  // namespace autofill
