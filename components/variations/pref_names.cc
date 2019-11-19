// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/pref_names.h"

namespace variations {
namespace prefs {

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

// The serialized base::Time from the last successful seed fetch (i.e. when the
// Variations server responds with 200 or 304). This is a client timestamp.
const char kVariationsLastFetchTime[] = "variations_last_fetch_time";

// Pair of <Chrome version string, country code string> representing the country
// used for filtering permanent consistency studies until the next time Chrome
// is updated.
const char kVariationsPermanentConsistencyCountry[] =
    "variations_permanent_consistency_country";

// A country code string representing the country used for filtering permanent
// consistency studies and will not be updated on Chrome updated. It can be
// changed via chrome://translate-internals and is intended for
// testing / developer use.
const char kVariationsPermanentOverriddenCountry[] =
    "variations_permanent_overridden_country";

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
// the time at which the last known "safe" seed was received, though it could
// potentially be a build timestamp instead, if the received date is unknown. An
// empty (default-constructed) base::Time if there is no known "safe" seed. This
// is a server-provided timestamp.
const char kVariationsSafeSeedDate[] = "variations_safe_seed_date";

// The serialized base::Time from the fetch corresponding to the safe seed, i.e.
// a copy of the last value stored to the |kVariationsLastFetchTime| pref that
// corresponded to the same seed contents as the safe seed. This is a client
// timestamp.
// Note: This pref was added about a milestone after most of the other safe seed
// prefs, and so might be missing for some clients that otherwise have safe seed
// data.
const char kVariationsSafeSeedFetchTime[] = "variations_safe_seed_fetch_time";

// The active client locale that was successfully used in association with the
// last known "safe" seed.
const char kVariationsSafeSeedLocale[] = "variations_safe_seed_locale";

// The country code used by the VariationsService for evaluating permanent
// consistency studies, that was successfully used in association with the last
// known "safe" seed.
const char kVariationsSafeSeedPermanentConsistencyCountry[] =
    "variations_safe_seed_permanent_consistency_country";

// The country code received by the VariationsService for evaluating studies,
// that was successfully used in association with the last known "safe" seed.
// This is the country code used for session consistency studies.
const char kVariationsSafeSeedSessionConsistencyCountry[] =
    "variations_safe_seed_session_consistency_country";

// The digital signature of the last known "safe" variations seed's binary data,
// base64-encoded. Empty if there is no known "safe" seed.
const char kVariationsSafeSeedSignature[] = "variations_safe_seed_signature";

// The serialized base::Time from the last seed received. This is a
// server-provided timestamp.
const char kVariationsSeedDate[] = "variations_seed_date";

// Digital signature of the binary variations seed data, base64-encoded.
const char kVariationsSeedSignature[] = "variations_seed_signature";

}  // namespace prefs
}  // namespace variations
