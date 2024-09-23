// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PROFILE_TOKEN_QUALITY_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PROFILE_TOKEN_QUALITY_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/types/strong_alias.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/signatures.h"

namespace autofill {

class AutofillField;
class AutofillProfile;
class FormStructure;
class PersonalDataManager;

// `ProfileTokenQuality` is associated with one `AutofillProfile` and tracks if
// the supported types of that profile are accepted or edited after filling.
// It is intended to collect observations on form submission using
// `AddObservationsForFilledForm()`.
//
// The observations (accessible via `GetObservationsForFieldType()`) can be used
// to understand of the quality of a supported type. This has applications for
// e.g. voting ("high quality votes") and detection of quasi-duplicate profiles.
// No applications are implemented yet.
//
// Observations are only stored for stored types. The observations of derived
// types use the observations of the corresponding stored type. When collecting
// an observation for a derived type, it is stored as an observation for the
// corresponding stored type.
// See `components/autofill/README.md` for details on stored and derived types.
class ProfileTokenQuality {
 public:
  // Describes the different types of observations, derived from an autofilled
  // field at form submission.
  // Keep in sync with AutofillProfileTokenQualityObservationType in
  // tools/metrics/histograms/enums.xml.
  enum class ObservationType : uint8_t {
    // An observation type that this client doesn't understand. This is possible
    // if a newer client synced a new enum value that this client doesn't
    // understand. Internally, the unknown value is stored as it's underlying
    // type - but it is exposed as `kUnknown`. See `Observation`.
    kUnknown = 0,
    // The autofilled value of a stored type was accepted as-filled.
    kAccepted = 1,
    // The autofilled value of a derived type was accepted as-filled. This is
    // relevant since observations are only stored for stored types. For
    // example, if a phone city code field is accepted, it counts as a partial
    // accept for the corresponding whole number (the stored type).
    kPartiallyAccepted = 2,
    // The autofilled value was edited to a value with Levenshtein distance at
    // most `kMaximumLevenshteinDistance` of the original value. Comparisons are
    // case-insensitive.
    kEditedToSimilarValue = 3,
    // The autofilled value was edited to equal (case-insensitively) a different
    // supported type of the same profile.
    kEditedToDifferentTokenOfSameProfile = 4,
    // The autofilled value was edited to equal (case-insensitively) the same
    // type of a different profile.
    kEditedToSameTokenOfOtherProfile = 5,
    // The autofilled value was edited to equal (case-insensitively) a different
    // supported type of a different profile.
    kEditedToDifferentTokenOfOtherProfile = 6,
    // The autofilled value was edited to an empty string.
    kEditedValueCleared = 7,
    // The autofilled value was edited in a different way not captured by the
    // enum values above. E.g, edited to some completely different value, that
    // doesn't occur in any profile.
    kEditedFallback = 8,
    kMaxValue = kEditedFallback
  };

  // For each stored type, only at most `kMaxObservationsPerToken` observations
  // are stored. When additional observations are observed, the oldest existing
  // observations are dropped (FIFO).
  static constexpr size_t kMaxObservationsPerToken = 10;

  // The maximum Levenshtein distance to consider for `kEditedToSimilarValue`
  // observations.
  static constexpr size_t kMaximumLevenshteinDistance = 2;

  // Initializes the `ProfileTokenQuality` for the `profile`. `profile` must
  // be non-null and outlive the `ProfileTokenQuality` instance.
  explicit ProfileTokenQuality(AutofillProfile* profile);
  ProfileTokenQuality(const ProfileTokenQuality& other);
  ~ProfileTokenQuality();

  bool operator==(const ProfileTokenQuality& other) const;

  // Derives an observation from every field of the `form_structure` that was
  // autofilled with the `profile_`. Only fields with no existing observation
  // for the same type are considered.
  // The observations are associated with the fields' `Type()`.
  // Because the set of types of the form is form identifying, some observations
  // are randomly dropped for privacy reasons (see `AddSubsetOfObservations()`).
  // The values of the `form_structure`'s fields represent the initial values
  // of those fields. To derive the observation types, the current value in the
  // fields is required. For this reason, the `form_data` is passed in.
  // The function does not persist new observation in the database.
  // The `pdm` is necessary to access the other profiles of the user and derive
  // observation types like `kEditedToSameTokenOfOtherProfile`.
  // The function returns true if at least one new observation was collected.
  // TODO(crbug.com/40227496): Get rid of the `form_data` parameter.
  bool AddObservationsForFilledForm(const FormStructure& form_structure,
                                    const FormData& form_data,
                                    const PersonalDataManager& pdm);

  // Collects observations using `AddObservationsForFilledForm()` for all
  // profiles that were used to autofill the form.
  // Persists any newly collected observations.
  static void SaveObservationsForFilledFormForAllSubmittedProfiles(
      const FormStructure& form_structure,
      const FormData& form_data,
      PersonalDataManager& pdm);

  // Returns all `ObservationType`s available for the `type`. The resulting
  // vector has at most `kMaxNumberOfObservations` items. It can contain
  // observations of type `kUnknown`, if a newer client synced observation types
  // that this client doesn't understand.
  // `type` must be a supported type of `AutofillProfile`. For a derived `type`,
  // the observations of the corresponding stored type are returned.
  std::vector<ObservationType> GetObservationTypesForFieldType(
      FieldType type) const;

  // Observations are stored together with their stored `type` in the database.
  // The observations for each stored type are serialized as a sequence of
  // integers. Each observation is represented using two uint8_ts, where the
  // first byte represents the `ObservationType` and the second byte the
  // `FormSignatureHash`.
  // Changing the encoding requires adding migration logic to `AutofillTable`.
  // Tested partially by autofill_table_unittest.cc.
  std::vector<uint8_t> SerializeObservationsForStoredType(FieldType type) const;
  void LoadSerializedObservationsForStoredType(
      FieldType type,
      base::span<const uint8_t> serialized_data);

  void set_profile(AutofillProfile* profile) {
    CHECK(profile);
    profile_ = *profile;
  }

  // Copy the observations for the `type` from `other`.
  void CopyObservationsForStoredType(FieldType type,
                                     const ProfileTokenQuality& other);

  // Resets all observations for the `type`.
  void ResetObservationsForStoredType(FieldType type);

  // Resets the observations for all tokens in which `profile_` and `other`
  // differ. This is used as a mechanism to reset outdated observations, by
  // setting `other` to the profile before modifications took place.
  void ResetObservationsForDifferingTokens(const AutofillProfile& other);

 private:
  friend class ProfileTokenQualityTestApi;
  // To send/receive observations to/from Sync, the entire `Observation`s need
  // to be accessed, not just the `ObservationType`. This is implemented in
  // terms of friend classes to keep the `form_hash`es private.
  friend class ContactInfoEntryDataSetter;
  friend class ContactInfoProfileSetter;

  // For every form and stored type, at most a single observation is stored
  // (among the `kMaxObservationsPerToken` observations stored in total).
  // To track for which forms an observation was already collected,
  // `Observation`s store a low entropy hash of the form-signature of the
  // submitted form-field.
  // The field-signature is not part of the hash for two reasons:
  // - The observations are associated with a type, which in most cases already
  //   identifies the field in the form.
  // - By including the field-signature, the hashes from different observations
  //   can be combined to identify the form.
  // Since `kMaxObservationsPerToken` is small, a low number of bits suffice.
  using FormSignatureHash =
      base::StrongAlias<struct FormSignatureHashTag, uint8_t>;

  // An observation, describing the type of observed observation and the form-
  // field it was collected from.
  struct Observation {
    // Internally, observation types are not stored as an `ObservationType`, but
    // as their underlying type. This way, new (future) observation types,
    // synced in from newer clients can be handled:
    // - Since the sync code uses proto2, the `ObservationType`s are synced as
    //   ints. Otherwise old clients would lose future observation types.
    // - Casting the int from sync to the enum class `ObservationType` would
    //   technically preserve it's value. But it would come at the downside of
    //   making switch statements with all cases non-exhaustive.
    // For this reason, it is preferred to store the `ObservationType`s as their
    // underlying type in the data model as well.
    // Getters expose unknown values as `kUnknown`.
    std::underlying_type_t<ObservationType> type;
    FormSignatureHash form_hash = FormSignatureHash(0);

    bool operator==(const Observation& other) const = default;
  };

  // Returns a low-entry hash of the `form_signature`.
  FormSignatureHash GetFormSignatureHash(FormSignature form_signature) const;

  // Adds the `observation` to the `observations_` for the stored type of
  // `type`. The oldest existing observation for that type is discarded, if
  // the limit of `kMaxObservationsPerToken` is exceeded.
  void AddObservation(FieldType type, Observation observation);

  // The set of types of the form is form identifying. To avoid tracking
  // browsing history, this function randomly drops 3 observations from all
  // possible `observations` that could be collected. For the non-dropped
  // observations, `AddObservation()` is called.
  // If there are less than 3 or more than 11 observations, at least 1 and at
  // most 8 are added.
  // Returns the number of observations added.
  size_t AddSubsetOfObservations(
      std::vector<std::pair<FieldType, Observation>> observations);

  // Deduces the `ObservationType` from a `field` that was autofilled with
  // `profile_`. `other_profiles` are all the other profiles that the user has
  // stored. `*profile_` should not be part of `other_profiles`.
  // Since the `field.value` represents the initial value of the field,
  // the `current_field_value` is required.
  // The `app_locale` is necessary to access the profile information via
  // `GetInfo()`, which are the values that Autofill fills.
  ObservationType GetObservationTypeFromField(
      const AutofillField& field,
      std::u16string_view current_field_value,
      const std::vector<const AutofillProfile*>& other_profiles,
      const std::string& app_locale) const;

  // The profile for which observations are collected.
  raw_ref<AutofillProfile> profile_;

  // Maps from `AutofillTable::GetDatabaseStoredTypesOfAutofillProfile()` to the
  // observations for this stored type. The following invariants hold for the
  // `circular_deque`:
  // - The observations are ordered from oldest (`front()`) to newest
  //   (`back()`).
  // - No more than `kMaxObservationsPerToken` elements are stored.
  base::flat_map<FieldType, base::circular_deque<Observation>> observations_;

  // When true, `AddSubsetOfObservations()` adds all observations.
  bool diable_randomization_for_testing_ = false;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PROFILE_TOKEN_QUALITY_H_
