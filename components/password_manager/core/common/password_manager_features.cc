// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/password_manager_features.h"

#include "base/feature_list.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"

namespace {
// This covers the 95th percentile on desktop platforms. See
// `PasswordManager.PasskeyRetrievalWaitDuration` metric.
// On Android the target is to get retrieval times under this threshold.
constexpr int kDefaultDelaySuggestionsTimeout = 4000;
}  // namespace

namespace password_manager::features {
// NOTE: It is strongly recommended to use UpperCamelCase style for feature
//       names, e.g. "MyGreatFeature".

// When enabled, will inform browser about filling through `FillField` as user
// input.
BASE_FEATURE(kActorLoginTreatFillingAsUserInput,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Only relevant when `kShowSuggestionsOnAutofocus` is on. This prevents
// suggestions from being shown while waiting for passkeys to become available,
// if the popup was triggered by autofocus without user interaction. It is
// enabled by default and can be turned off if it is found to cause any
// problems.
BASE_FEATURE(kDelaySuggestionsOnAutofocusWaitingForPasskeys,
             "DelaysSuggestionsOnAutofocusWaitingForPasskeys",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Timeout value used when kDelaySuggestionsOnAutofocusWaitingForPasskeys is
// enabled.
BASE_FEATURE_PARAM(int,
                   kDelaySuggestionsOnAutofocusTimeout,
                   &kDelaySuggestionsOnAutofocusWaitingForPasskeys,
                   "timeout_ms",
                   kDefaultDelaySuggestionsTimeout);

// Removes password suggestion filtering by username.
BASE_FEATURE(kNoPasswordSuggestionFiltering, base::FEATURE_DISABLED_BY_DEFAULT);

// Allows to show suggestions automatically when password forms are autofocused
// on pageload. Enabled by default on desktop in M140.
BASE_FEATURE(kShowSuggestionsOnAutofocus,
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Field trial identifier for password generation requirements.
const char kGenerationRequirementsFieldTrial[] =
    "PasswordGenerationRequirements";

// The file version number of password requirements files. If the prefix length
// changes, this version number needs to be updated.
// Default to 0 in order to get an empty requirements file.
const char kGenerationRequirementsVersion[] = "version";

// Length of a hash prefix of domain names. This is used to shard domains
// across multiple files.
// Default to 0 in order to put all domain names into the same shard.
const char kGenerationRequirementsPrefixLength[] = "prefix_length";

// Timeout (in milliseconds) for password requirements lookups. As this is a
// network request in the background that does not block the UI, the impact of
// high values is not strong.
// Default to 5000 ms.
const char kGenerationRequirementsTimeout[] = "timeout";

}  // namespace password_manager::features
