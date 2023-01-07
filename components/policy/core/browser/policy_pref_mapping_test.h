// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_POLICY_PREF_MAPPING_TEST_H_
#define COMPONENTS_POLICY_CORE_BROWSER_POLICY_PREF_MAPPING_TEST_H_

#include <stddef.h>

namespace base {
class FilePath;
}

namespace policy {
class MockConfigurationPolicyProvider;
}

class PrefService;

namespace policy {

// Allows to split the test cases for VerifyPolicyToPrefMappings into individual
// chunks since the test might otherwise time out.
struct PrefMappingChunkInfo {
  size_t current_chunk;
  size_t num_chunks;
};

// The name of the switch to filter the testcases by
// ${policy_name}[.optionalTestNameSuffix]. Several names could be passed
// separated by colon. (For example --test_policy_to_pref_mappings_filter=\
// AuthNegotiateDelegateByKdcPolicy:\
// BuiltInDnsClientEnabled.FeatureEnabledByDefault
// Run with --gtest_filter=*PolicyToPrefsMapping/0. When the filter is specified
// - the number of chunks is set to 1.
inline constexpr char kPolicyToPrefMappingsFilterSwitch[] =
    "test_policy_to_pref_mappings_filter";

// Verifies that all of the policies have a test case listed in the JSON file at
// |test_case_path|.
void VerifyAllPoliciesHaveATestCase(const base::FilePath& test_case_path);

// Verifies that policies make their corresponding preferences become managed,
// and that the user can't override that setting.  Loads test cases from the
// JSON file at |test_case_path| and updates policies using the given
// |provider|.
// |local_state|, |user_prefs| or |signin_profile_prefs| can be nullptr, in
// which case the mappings into the respective location are skipped.
// TODO(https://crbug.com/809991) Policies mapping into CrosSettings are always
// skipped.
void VerifyPolicyToPrefMappings(const base::FilePath& test_case_path,
                                PrefService* local_state,
                                PrefService* user_prefs,
                                PrefService* signin_profile_prefs,
                                MockConfigurationPolicyProvider* provider,
                                PrefMappingChunkInfo* chunk_info = nullptr);

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_POLICY_PREF_MAPPING_TEST_H_
