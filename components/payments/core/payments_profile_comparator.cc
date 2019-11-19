// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payments_profile_comparator.h"

#include <algorithm>
#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/validation.h"
#include "components/payments/core/payment_options_provider.h"
#include "components/payments/core/payment_request_data_util.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/libaddressinput/chromium/addressinput_util.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "ui/base/l10n/l10n_util.h"

namespace payments {

PaymentsProfileComparator::PaymentsProfileComparator(
    const std::string& app_locale,
    const PaymentOptionsProvider& options)
    : autofill::AutofillProfileComparator(app_locale), options_(options) {}

PaymentsProfileComparator::~PaymentsProfileComparator() {}

PaymentsProfileComparator::ProfileFields
PaymentsProfileComparator::GetMissingProfileFields(
    const autofill::AutofillProfile* profile) const {
  if (!profile)
    return kName | kPhone | kEmail | kAddress;

  if (!cache_.count(profile->guid())) {
    cache_[profile->guid()] = ComputeMissingFields(*profile);
  } else {
    // Cache hit. In debug mode, recompute and check that invalidation has
    // occurred where necessary.
    DCHECK_EQ(cache_[profile->guid()], ComputeMissingFields(*profile))
        << "Profiles must be invalidated when their contents change.";
  }

  return cache_[profile->guid()];
}

std::vector<autofill::AutofillProfile*>
PaymentsProfileComparator::FilterProfilesForContact(
    const std::vector<autofill::AutofillProfile*>& profiles) const {
  // We will be removing profiles, so we operate on a copy.
  std::vector<autofill::AutofillProfile*> processed = profiles;

  // Stable sort, since profiles are expected to be passed in frecency order.
  std::stable_sort(
      processed.begin(), processed.end(),
      [this](autofill::AutofillProfile* p1, autofill::AutofillProfile* p2) {
        return GetContactCompletenessScore(p1) >
               GetContactCompletenessScore(p2);
      });

  auto it = processed.begin();
  while (it != processed.end()) {
    if (GetContactCompletenessScore(*it) == 0) {
      // Since profiles are sorted by completeness, this and any further
      // profiles can be discarded.
      processed.erase(it, processed.end());
      break;
    }

    // Attempt to find a matching element in the vector before the current.
    // This is quadratic, but the number of elements is generally small
    // (< 10), so a more complicated algorithm would be overkill.
    if (std::find_if(processed.begin(), it,
                     [&](autofill::AutofillProfile* prior) {
                       return IsContactEqualOrSuperset(*prior, **it);
                     }) != it) {
      // Remove the subset profile. |it| will point to the next element after
      // erasure.
      it = processed.erase(it);
    } else {
      it++;
    }
  }

  return processed;
}

bool PaymentsProfileComparator::IsContactEqualOrSuperset(
    const autofill::AutofillProfile& super,
    const autofill::AutofillProfile& sub) const {
  if (options_.request_payer_name()) {
    if (sub.HasInfo(autofill::NAME_FULL) &&
        !super.HasInfo(autofill::NAME_FULL)) {
      return false;
    }
    if (!HaveMergeableNames(super, sub))
      return false;
  }
  if (options_.request_payer_phone()) {
    if (sub.HasInfo(autofill::PHONE_HOME_WHOLE_NUMBER) &&
        !super.HasInfo(autofill::PHONE_HOME_WHOLE_NUMBER)) {
      return false;
    }
    if (!HaveMergeablePhoneNumbers(super, sub))
      return false;
  }
  if (options_.request_payer_email()) {
    if (sub.HasInfo(autofill::EMAIL_ADDRESS) &&
        !super.HasInfo(autofill::EMAIL_ADDRESS)) {
      return false;
    }
    if (!HaveMergeableEmailAddresses(super, sub))
      return false;
  }
  return true;
}

int PaymentsProfileComparator::GetContactCompletenessScore(
    const autofill::AutofillProfile* profile) const {
  // Create a bitmask of the fields that are both present and required.
  ProfileFields present =
      ~GetMissingProfileFields(profile) & GetRequiredProfileFieldsForContact();

  // Count how many are set.
  return !!(present & kName) + !!(present & kPhone) + !!(present & kEmail);
}

bool PaymentsProfileComparator::IsContactInfoComplete(
    const autofill::AutofillProfile* profile) const {
  // Mask the fields that are missing with those that are requried. If any bits
  // are set (i.e., the result is nonzero), then contact info is incomplete.
  return !(GetMissingProfileFields(profile) &
           GetRequiredProfileFieldsForContact());
}

std::vector<autofill::AutofillProfile*>
PaymentsProfileComparator::FilterProfilesForShipping(
    const std::vector<autofill::AutofillProfile*>& profiles) const {
  // Since we'll be changing the order/contents of the const input vector,
  // we make a copy.
  std::vector<autofill::AutofillProfile*> processed = profiles;

  std::stable_sort(
      processed.begin(), processed.end(),
      [this](autofill::AutofillProfile* p1, autofill::AutofillProfile* p2) {
        return GetShippingCompletenessScore(p1) >
               GetShippingCompletenessScore(p2);
      });

  // TODO(crbug.com/722949): Remove profiles with no relevant information, or
  // which are subsets of more-complete profiles.

  return processed;
}

int PaymentsProfileComparator::GetShippingCompletenessScore(
    const autofill::AutofillProfile* profile) const {
  // Create a bitmask of the fields that are both present and required.
  ProfileFields present =
      ~GetMissingProfileFields(profile) & GetRequiredProfileFieldsForShipping();

  // Count how many are set. The completeness of the address is weighted so as
  // to dominate the other fields.
  return !!(present & kName) + !!(present & kPhone) +
         (10 * !!(present & kAddress));
}

bool PaymentsProfileComparator::IsShippingComplete(
    const autofill::AutofillProfile* profile) const {
  // Mask the fields that are missing with those that are requried. If any bits
  // are set (i.e., the result is nonzero), then shipping is incomplete.
  return !(GetMissingProfileFields(profile) &
           GetRequiredProfileFieldsForShipping());
}

void PaymentsProfileComparator::RecordMissingFieldsOfShippingProfile(
    const autofill::AutofillProfile* profile) const {
  // We should not record anything when no shipping fields is required.
  if (GetRequiredProfileFieldsForShipping() == kNone)
    return;

  // Record any required fields that are missing.
  PaymentsProfileComparator::ProfileFields missing_fields =
      GetMissingProfileFields(profile) & GetRequiredProfileFieldsForShipping();
  if (missing_fields != kNone) {
    base::UmaHistogramSparse("PaymentRequest.MissingShippingFields",
                             missing_fields);
  }
}

void PaymentsProfileComparator::RecordMissingFieldsOfContactProfile(
    const autofill::AutofillProfile* profile) const {
  // We should not record anything when no contact fields is required.
  if (GetRequiredProfileFieldsForContact() == kNone)
    return;

  // Record any required fields that are missing.
  PaymentsProfileComparator::ProfileFields missing_fields =
      GetMissingProfileFields(profile) & GetRequiredProfileFieldsForContact();
  if (missing_fields != kNone) {
    base::UmaHistogramSparse("PaymentRequest.MissingContactFields",
                             missing_fields);
  }
}

base::string16 PaymentsProfileComparator::GetStringForMissingContactFields(
    const autofill::AutofillProfile& profile) const {
  return GetStringForMissingFields(GetMissingProfileFields(&profile) &
                                   GetRequiredProfileFieldsForContact());
}

base::string16 PaymentsProfileComparator::GetTitleForMissingContactFields(
    const autofill::AutofillProfile& profile) const {
  return GetTitleForMissingFields(GetMissingProfileFields(&profile) &
                                  GetRequiredProfileFieldsForContact());
}

base::string16 PaymentsProfileComparator::GetStringForMissingShippingFields(
    const autofill::AutofillProfile& profile) const {
  return GetStringForMissingFields(GetMissingProfileFields(&profile) &
                                   GetRequiredProfileFieldsForShipping());
}

base::string16 PaymentsProfileComparator::GetTitleForMissingShippingFields(
    const autofill::AutofillProfile& profile) const {
  return GetTitleForMissingFields(GetMissingProfileFields(&profile) &
                                  GetRequiredProfileFieldsForShipping());
}

void PaymentsProfileComparator::Invalidate(
    const autofill::AutofillProfile& profile) {
  cache_.erase(profile.guid());
}

PaymentsProfileComparator::ProfileFields
PaymentsProfileComparator::ComputeMissingFields(
    const autofill::AutofillProfile& profile) const {
  ProfileFields missing = kNone;

  if (!profile.HasInfo(autofill::NAME_FULL))
    missing |= kName;

  // Determine the country code to use when validating the phone number. Use
  // the profile's country if it has one, or the code for the app locale
  // otherwise. Note that international format numbers will always work--this
  // is just the region that will be used to check if the number is
  // potentially in a local format.
  const std::string country =
      autofill::data_util::GetCountryCodeWithFallback(profile, app_locale());

  base::string16 phone = profile.GetInfo(
      autofill::AutofillType(autofill::PHONE_HOME_WHOLE_NUMBER), app_locale());
  base::string16 intl_phone = base::UTF8ToUTF16("+" + base::UTF16ToUTF8(phone));
  if (!(autofill::IsPossiblePhoneNumber(phone, country) ||
        autofill::IsPossiblePhoneNumber(intl_phone, country)))
    missing |= kPhone;

  base::string16 email = profile.GetInfo(
      autofill::AutofillType(autofill::EMAIL_ADDRESS), app_locale());
  if (!autofill::IsValidEmailAddress(email))
    missing |= kEmail;

  if (!AreRequiredAddressFieldsPresent(profile))
    missing |= kAddress;

  return missing;
}

PaymentsProfileComparator::ProfileFields
PaymentsProfileComparator::GetRequiredProfileFieldsForContact() const {
  ProfileFields required = kNone;
  if (options_.request_payer_name())
    required |= kName;
  if (options_.request_payer_phone())
    required |= kPhone;
  if (options_.request_payer_email())
    required |= kEmail;
  return required;
}

PaymentsProfileComparator::ProfileFields
PaymentsProfileComparator::GetRequiredProfileFieldsForShipping() const {
  return options_.request_shipping() ? (kAddress | kName | kPhone) : kNone;
}

base::string16 PaymentsProfileComparator::GetStringForMissingFields(
    PaymentsProfileComparator::ProfileFields fields) const {
  switch (fields) {
    case kNone:
      // No bits are set, so no fields are missing.
      return base::string16();
    case kName:
      return l10n_util::GetStringUTF16(IDS_PAYMENTS_NAME_REQUIRED);
    case kPhone:
      return l10n_util::GetStringUTF16(IDS_PAYMENTS_PHONE_NUMBER_REQUIRED);
    case kEmail:
      return l10n_util::GetStringUTF16(IDS_PAYMENTS_EMAIL_REQUIRED);
    case kAddress:
      return l10n_util::GetStringUTF16(IDS_PAYMENTS_INVALID_ADDRESS);
    default:
      // Either multiple bits are set (likely) or one bit that doesn't
      // correspond to a named constant is set (shouldn't happen). Return a
      // generic "More information" message.
      return l10n_util::GetStringUTF16(IDS_PAYMENTS_MORE_INFORMATION_REQUIRED);
  }
}

base::string16 PaymentsProfileComparator::GetTitleForMissingFields(
    PaymentsProfileComparator::ProfileFields fields) const {
  switch (fields) {
    case 0:
      NOTREACHED() << "Title should not be requested if no fields are missing";
      return base::string16();
    case kName:
      return l10n_util::GetStringUTF16(IDS_PAYMENTS_ADD_NAME);
    case kPhone:
      return l10n_util::GetStringUTF16(IDS_PAYMENTS_ADD_PHONE_NUMBER);
    case kEmail:
      return l10n_util::GetStringUTF16(IDS_PAYMENTS_ADD_EMAIL);
    case kAddress:
      return l10n_util::GetStringUTF16(IDS_PAYMENTS_ADD_VALID_ADDRESS);
    default:
      // Either multiple bits are set (likely) or one bit that doesn't
      // correspond to a named constant is set (shouldn't happen). Return a
      // generic "More information" message.
      return l10n_util::GetStringUTF16(IDS_PAYMENTS_ADD_MORE_INFORMATION);
  }
}

bool PaymentsProfileComparator::AreRequiredAddressFieldsPresent(
    const autofill::AutofillProfile& profile) const {
  std::unique_ptr<::i18n::addressinput::AddressData> data =
      autofill::i18n::CreateAddressDataFromAutofillProfile(profile,
                                                           app_locale());

  return autofill::addressinput::HasAllRequiredFields(*data);
}

}  // namespace payments
