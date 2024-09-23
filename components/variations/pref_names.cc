// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/pref_names.h"

namespace variations {
namespace prefs {

// Reflects the state of the "DeviceChromeVariations" policy. The policy
// determines if and which variations should be enabled for the client on
// ChromeOS. The possible values are defined in the
// variations::RestrictionPolicy enum.
const char kDeviceVariationsRestrictionsByPolicy[] =
    "device_variations_restrictions_by_policy";

// base64-encoded compressed serialized form of the variations seed protobuf.
const char kVariationsCompressedSeed[] = "variations_compressed_seed";

// The latest country code received by the VariationsService for evaluating
// studies.
const char kVariationsCountry[] = "variations_country";

// The number of times that Chrome has crashed before successfully fetching a
// new seed. Used to determine whether to fall back to a "safe" seed.
const char kVariationsCrashStreak[] = "variations_crash_streak";

// The number of times that the VariationsService has failed to fetch a new
// seed. Used to determine whether to fall back to a "safe" seed.
const char kVariationsFailedToFetchSeedStreak[] =
    "variations_failed_to_fetch_seed_streak";

// Local-state preference containing a dictionary of profile names to a list of
// gaia IDs.  For example:
//
// "variations_google_groups": {
//   "Profile 1": [ "123456", "2345678" ],
//   "Profile 4": [ ]
// }
//
// This pref used as follows.
// * Written to by a pref observer based on per-profile sync data. This pref is
//   a profile-keyed dictionary so it can be updated based only on the new value
//   of a single profile's groups.
// * Read by variations code when processing the finch seed at startup. This
//   code cares only about the union of the groups across all profiles.
const char kVariationsGoogleGroups[] = "variations_google_groups";

// The serialized base::Time from the last successful seed fetch (i.e. when the
// Variations server responds with 200 or 304). This is a client timestamp.
const char kVariationsLastFetchTime[] = "variations_last_fetch_time";

// The milestone, e.g. 96, with which the regular seed was fetched.
const char kVariationsSeedMilestone[] = "variations_seed_milestone";

// Pair of <Chrome version string, country code string> representing the country
// used for filtering permanent consistency studies until the next time Chrome
// is updated.
const char kVariationsPermanentConsistencyCountry[] =
    "variations_permanent_consistency_country";

// A country code string representing the country used for filtering permanent
// consistency studies. This is not updated when Chrome is updated, but it can
// be changed via chrome://translate-internals and is intended for testing and
// developer use.
const char kVariationsPermanentOverriddenCountry[] =
    "variations_permanent_overridden_country";

// Reflects the state of the "ChromeVariations" policy. The policy determines if
// and which variations should be enabled for the client. The possible values
// are defined in the variations::RestrictionPolicy enum.
const char kVariationsRestrictionsByPolicy[] =
    "variations_restrictions_by_policy";

// String for the restrict parameter to be appended to the variations URL.
const char kVariationsRestrictParameter[] = "variations_restrict_parameter";

// The last known "safe" variations seed, stored as the result of compressing
// the base64-encoded serialized form of the variations seed protobuf. Empty if
// there is no known "safe" seed. A seed is deemed "safe" if, while the seed is
// active, it has been observed to be possible to reach the variations server
// and download a new seed. Design doc:
// https://docs.google.com/document/d/17UN2pLSa5JZqk8f3LeYZIftXewxqcITotgalTrJvGSY
const char kVariationsSafeCompressedSeed[] = "variations_safe_compressed_seed";

// The serialized base::Time used for safe seed expiry checks. This is usually
// the time at which the last known "safe" seed was received; however, it could
// be one of the following:
// (A) A build timestamp if the received date is unknown.
// (B) A client-provided timestamp set during the FRE on select platforms in
//     ChromeFeatureListCreator::SetupInitialPrefs() when the client fetches a
//     seed from a Variations server and the regular seed is promoted to the
//     safe seed.
// (C) An empty (default-constructed) base::Time if there is no known "safe"
//     seed.
//
// This is a server-provided timestamp unless it stores (B).
const char kVariationsSafeSeedDate[] = "variations_safe_seed_date";

// The serialized base::Time from the fetch corresponding to the safe seed, i.e.
// a copy of the last value stored in the |kVariationsLastFetchTime| pref that
// corresponded to the same seed contents as the safe seed. This is a client
// timestamp.
// Note: This pref was added about a milestone after most of the other safe seed
// prefs, so it might be missing for some clients that otherwise have safe seed
// data.
const char kVariationsSafeSeedFetchTime[] = "variations_safe_seed_fetch_time";

// The active client locale that was successfully used in association with the
// last known "safe" seed.
const char kVariationsSafeSeedLocale[] = "variations_safe_seed_locale";

// The milestone with which the "safe" seed was fetched.
const char kVariationsSafeSeedMilestone[] = "variations_safe_seed_milestone";

// The seed that is used to randomize the limited entropy synthetic trial.
// Previously this was called "variations_limited_entropy_synthetic_trial_seed".
// It was renamed to fix an imbalance in the `LimitedEntropySyntheticTrial`.
// TODO(crbug.com/40948861): Remove both this and the old pref value after the
// synthetic trial wraps up.
const char kVariationsLimitedEntropySyntheticTrialSeed[] =
    "variations_limited_entropy_synthetic_trial_seed_v2";

// A saved copy of |kVariationsPermanentConsistencyCountry|. The saved value is
// the most recent value that was successfully used by the VariationsService for
// evaluating permanent consistency studies.
const char kVariationsSafeSeedPermanentConsistencyCountry[] =
    "variations_safe_seed_permanent_consistency_country";

// A saved copy of |kVariationsCountry|. The saved value is the most recent
// value that was successfully used by the VariationsService for evaluating
// session consistency studies.
const char kVariationsSafeSeedSessionConsistencyCountry[] =
    "variations_safe_seed_session_consistency_country";

// The digital signature of the last known "safe" variations seed's binary data,
// base64-encoded. Empty if there is no known "safe" seed.
const char kVariationsSafeSeedSignature[] = "variations_safe_seed_signature";

// The serialized base::Time from the last seed received. This is a
// server-provided timestamp.
//
// On select platforms, this is set to a client-provided timestamp until a seed
// is fetched from a Variations server and the pref is updated with a
// server-provided timestamp. See ChromeFeatureListCreator::SetupInitialPrefs().
const char kVariationsSeedDate[] = "variations_seed_date";

// Digital signature of the binary variations seed data, base64-encoded.
const char kVariationsSeedSignature[] = "variations_seed_signature";

// Stores the list of field trials forced by field-trial-internals.
const char kVariationsForcedFieldTrials[] = "variations_forced_field_trials";

// The expiration time for all forced field trials.
// See components/variations/field_trial_internals_utils.h for more detail.
const char kVariationsForcedTrialExpiration[] =
    "variations_forced_trial_expiration";

// Number of Chrome starts which have occurred after forcing field trials.
// Forced trials are automatically stopped after a few Chrome starts,
// See components/variations/field_trial_internals_utils.h for more detail.
const char kVariationsForcedTrialStarts[] = "variations_forced_trial_starts";

}  // namespace prefs
}  // namespace variations
