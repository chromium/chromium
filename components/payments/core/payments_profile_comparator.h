// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_PAYMENTS_PROFILE_COMPARATOR_H_
#define COMPONENTS_PAYMENTS_CORE_PAYMENTS_PROFILE_COMPARATOR_H_

#include <map>
#include <string>
#include <vector>

#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"

// Utility functions used for processing and filtering address profiles
// (AutofillProfile).

namespace autofill {
class AutofillProfile;
}  // namespace autofill

namespace payments {

class PaymentOptionsProvider;

// Helper class which evaluates profiles for similarity and completeness.
// Profiles are evaluated once for completeness, and the result is cached,
// meaning one instance of this class should be used per-request to avoid
// redoing expensive validation checks.
// Note that, if a profile is modified and saved during the course of the
// PaymentRequest, it is important to call the Invalidate method to ensure
// it is properly evaluated.
class PaymentsProfileComparator : public autofill::AutofillProfileComparator {
 public:
  // Bitmask of potentially-required fields used in evaluating completeness. Bit
  // field values are identical to CompletionStatus in AutofillAddress.java and
  // ContactEditor.java. Please also modify java files after changing these bits
  // since missing fields on both Android and Desktop are recorded in the same
  // UMA metric: PaymentRequest.Missing[Shipping|Contact]Fields.
  using ProfileFields = uint32_t;
  const static ProfileFields kNone = 0;
  const static ProfileFields kName = 1 << 0;
  const static ProfileFields kPhone = 1 << 1;
  const static ProfileFields kEmail = 1 << 2;
  const static ProfileFields kAddress = 1 << 3;

  PaymentsProfileComparator(const std::string& app_locale,
                            const PaymentOptionsProvider& options);
  virtual ~PaymentsProfileComparator();

  // Returns a bitmask indicating which fields (or groups of fields) on this
  // profile are not complete and valid.
  ProfileFields GetMissingProfileFields(
      const autofill::AutofillProfile* profile) const;

  // Returns profiles for contact info, ordered by completeness and
  // deduplicated. |profiles| should be passed in order of frecency, and this
  // order will be preserved among equally-complete profiles. Deduplication here
  // means that profiles returned are excluded if they are a subset of a more
  // complete or more frecent profile. Completeness here refers only to the
  // presence of the fields requested per the request_payer_* fields in
  // |options|.
  std::vector<autofill::AutofillProfile*> FilterProfilesForContact(
      const std::vector<autofill::AutofillProfile*>& profiles) const;

  // Returns true iff all of the contact info in |sub| also appears in |super|.
  // Only operates on fields requested in |options|.
  bool IsContactEqualOrSuperset(const autofill::AutofillProfile& super,
                                const autofill::AutofillProfile& sub) const;

  // Returns the number of contact fields requested in |options| which are
  // nonempty in |profile|.
  int GetContactCompletenessScore(
      const autofill::AutofillProfile* profile) const;

  // Returns true iff every contact field requested in |options| is nonempty in
  // |profile|.
  bool IsContactInfoComplete(const autofill::AutofillProfile* profile) const;

  // Returns profiles for shipping, ordered by completeness. |profiles| should
  // be passed in order of frecency, and this order will be preserved among
  // equally-complete profiles.
  std::vector<autofill::AutofillProfile*> FilterProfilesForShipping(
      const std::vector<autofill::AutofillProfile*>& profiles) const;

  int GetShippingCompletenessScore(
      const autofill::AutofillProfile* profile) const;

  // Returns true iff every field needed to use |profile| as a shipping address
  // is populated.
  bool IsShippingComplete(const autofill::AutofillProfile* profile) const;

  // Returns a localized string to be displayed in UI indicating what action,
  // if any, must be taken for the given profile to be used as contact info.
  base::string16 GetStringForMissingContactFields(
      const autofill::AutofillProfile& profile) const;

  // Returns a localized string to be displayed as the title of a piece of UI,
  // indicating what action must be taken for the given profile to be used as
  // contact info.
  base::string16 GetTitleForMissingContactFields(
      const autofill::AutofillProfile& profile) const;

  // Returns a localized string to be displayed in UI indicating what action,
  // if any, must be taken for the given profile to be used as a shipping
  // address.
  base::string16 GetStringForMissingShippingFields(
      const autofill::AutofillProfile& profile) const;

  // Returns a localized string to be displayed as the title of a piece of UI,
  // indicating what action must be taken for the given profile to be used as
  // shipping address.
  base::string16 GetTitleForMissingShippingFields(
      const autofill::AutofillProfile& profile) const;

  void RecordMissingFieldsOfShippingProfile(
      const autofill::AutofillProfile* profile) const;
  void RecordMissingFieldsOfContactProfile(
      const autofill::AutofillProfile* profile) const;

  // Clears the cached evaluation result for |profile|. Must be called when a
  // profile is modified and saved during the course of a PaymentRequest.
  virtual void Invalidate(const autofill::AutofillProfile& profile);

 private:
  ProfileFields ComputeMissingFields(
      const autofill::AutofillProfile& profile) const;
  ProfileFields GetRequiredProfileFieldsForContact() const;
  ProfileFields GetRequiredProfileFieldsForShipping() const;
  base::string16 GetStringForMissingFields(ProfileFields fields) const;
  base::string16 GetTitleForMissingFields(ProfileFields fields) const;
  bool AreRequiredAddressFieldsPresent(
      const autofill::AutofillProfile& profile) const;

  mutable std::map<std::string, ProfileFields> cache_;
  const PaymentOptionsProvider& options_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_PAYMENTS_PROFILE_COMPARATOR_H_
