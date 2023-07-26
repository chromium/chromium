// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PROFILE_TOKEN_QUALITY_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PROFILE_TOKEN_QUALITY_H_

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/allocator/partition_allocator/pointers/raw_ptr.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/types/strong_alias.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/signatures.h"

namespace autofill {

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
//
// TODO(crbug.com/1453650): Implement the class, make it a member of
// `AutofillProfile` and start collecting observations.
// TODO(crbug.com/1453650): Interface + logic to reset observations for
// `kAccount` profiles and for edits via settings.
// TODO(crbug.com/1453650): Interface + logic to initialize the class with
// existing observations (when loading it from the DB).
// TODO(crbug.com/1453650): Treat values entered through settings as `kAccepted`
// observations.
class ProfileTokenQuality {
 public:
  // Describes the different types of observations, derived from an autofilled
  // field at form submission.
  enum class ObservationType {
    // Default value. Not used for actual observations.
    kNone = 0,
    // The autofilled value of a stored type was accepted as-filled.
    kAccepted = 1,
    // The autofilled value of a derived type was accepted as-filled. This is
    // relevant since observations are only stored for stored types. For
    // example, if a phone city code field is accepted, it counts as a partial
    // accept for the corresponding whole number (the stored type).
    kPartiallyAccepted = 2,
    // The autofilled value was edited to a value with small edit distance to
    // the original value. Comparisons are case-insensitive.
    // TODO(crbug.com/1453650): Decide what a "small" edit distance is.
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
  };

  // For each stored type, only at most `kMaxObservationsPerToken` observations
  // are stored. When additional observations are observed, the oldest existing
  // observations are dropped (FIFO).
  static constexpr size_t kMaxObservationsPerToken = 10;

  // Initializes the `ProfileTokenQuality` for the `profile`. `profile` must
  // be non-null and outlive the `ProfileTokenQuality` instance.
  explicit ProfileTokenQuality(AutofillProfile* profile);
  ~ProfileTokenQuality();

  // Derives an observation from every field of the `form_structure` that was
  // autofilled with the `profile_`. Only fields with no existing observation
  // for the same type are considered.
  // The observations are associated with the fields' `Type()`.
  // The values of the `form_structure`'s fields represent the initial values
  // of those fields. To derive the observation types, the current value in the
  // fields is required. For this reason, the `form_data` is passed in.
  // The function does not persist new observation in the database.
  // The `pdm` is necessary to access the other profiles of the user and derive
  // observation types like `kEditedToSameTokenOfOtherProfile`.
  // The function returns true if at least one new observation was collected.
  // TODO(crbug.com/1331312): Get rid of the `form_data` parameter.
  bool AddObservationsForFilledForm(const FormStructure& form_structure,
                                    const FormData& form_data,
                                    const PersonalDataManager& pdm);

  void AddObservationForTesting(ServerFieldType field_type,
                                ObservationType observation_type);

  // Returns all `ObservationType`s available for the `type`. The resulting
  // vector has at most `kMaxNumberOfObservations` items. It is guaranteed that
  // no observation type is `kNone`.
  // `type` must be a supported type of `AutofillProfile`. For a derived `type`,
  // the observations of the corresponding stored type are returned.
  std::vector<ObservationType> GetObservationTypesForFieldType(
      ServerFieldType type) const;

 private:
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
    ObservationType type = ObservationType::kNone;
    FormSignatureHash form_hash = FormSignatureHash(0);
  };

  // Returns a low-entry hash of the `form_signature`.
  FormSignatureHash GetFormSignatureHash(FormSignature form_signature) const;

  // Adds the `observation` to the `observations_` for the stored type of
  // `type`. The oldest existing observation for that type is discarded, if
  // the limit of `kMaxObservationsPerToken` is exceeded.
  void AddObservation(ServerFieldType type, Observation observation);

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
      const std::vector<AutofillProfile*>& other_profiles,
      const std::string& app_locale) const;

  // The profile for which observations are collected. Not null.
  base::raw_ptr<AutofillProfile> profile_;

  // Maps from `AutofillTable::GetStoredTypesForAutofillProfile()` to the
  // observations for this stored type. The following invariants hold for the
  // `circular_deque`:
  // - The observations are ordered from oldest (`front()`) to newest
  //   (`back()`).
  // - No stored observation has type `kNone`.
  // - No more than `kMaxObservationsPerToken` elements are stored.
  base::flat_map<ServerFieldType, base::circular_deque<Observation>>
      observations_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PROFILE_TOKEN_QUALITY_H_
