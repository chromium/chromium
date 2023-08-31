// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/policy_pref_mapping_test.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/hash/hash.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "printing/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace policy {

namespace {

// The name of the instructions key in policy_test_cases.json that does not need
// to be parsed.
const char kInstructionKeyName[] = "-- Instructions --";

enum class PrefLocation {
  kUserProfile,
  kSigninProfile,
  kLocalState,
};

PrefLocation GetPrefLocation(const base::Value::Dict& settings) {
  const std::string* location = settings.FindString("location");
  if (!location || *location == "user_profile")
    return PrefLocation::kUserProfile;
  if (*location == "local_state")
    return PrefLocation::kLocalState;
  if (*location == "signin_profile")
    return PrefLocation::kSigninProfile;
  ADD_FAILURE() << "Unknown pref location: " << *location;
  return PrefLocation::kUserProfile;
}

std::string GetPolicyName(const std::string& policy_name_decorated) {
  const size_t offset = policy_name_decorated.find('.');
  if (offset != std::string::npos)
    return policy_name_decorated.substr(0, offset);
  return policy_name_decorated;
}

PrefService* GetPrefServiceForLocation(PrefLocation location,
                                       PrefService* local_state,
                                       PrefService* user_prefs,
                                       PrefService* signin_profile_prefs) {
  switch (location) {
    case PrefLocation::kUserProfile:
      return user_prefs;
    case PrefLocation::kSigninProfile:
      return signin_profile_prefs;
    case PrefLocation::kLocalState:
      return local_state;
    default:
      ADD_FAILURE() << "Unhandled pref location: "
                    << static_cast<int>(location);
  }
  return nullptr;
}

void CheckPrefHasValue(const PrefService::Preference* pref,
                       const base::Value* expected_value) {
  ASSERT_TRUE(pref);

  const base::Value* pref_value = pref->GetValue();
  ASSERT_TRUE(pref->GetValue());
  ASSERT_TRUE(expected_value);
  EXPECT_EQ(*pref_value, *expected_value);
}

void CheckPrefHasDefaultValue(const PrefService::Preference* pref,
                              const base::Value* expected_value = nullptr) {
  ASSERT_TRUE(pref);
  EXPECT_TRUE(pref->IsDefaultValue());
  EXPECT_TRUE(pref->IsUserModifiable());
  EXPECT_FALSE(pref->IsUserControlled());
  EXPECT_FALSE(pref->IsManaged());
  EXPECT_FALSE(pref->IsRecommended());
  if (expected_value)
    CheckPrefHasValue(pref, expected_value);
}

void CheckPrefHasRecommendedValue(const PrefService::Preference* pref,
                                  const base::Value* expected_value) {
  ASSERT_TRUE(pref);
  ASSERT_TRUE(expected_value);
  EXPECT_FALSE(pref->IsDefaultValue());
  EXPECT_TRUE(pref->IsUserModifiable());
  EXPECT_FALSE(pref->IsUserControlled());
  EXPECT_FALSE(pref->IsManaged());
  EXPECT_TRUE(pref->IsRecommended());
  CheckPrefHasValue(pref, expected_value);
}

void CheckPrefHasMandatoryValue(const PrefService::Preference* pref,
                                const base::Value* expected_value) {
  ASSERT_TRUE(pref);
  ASSERT_TRUE(expected_value);
  EXPECT_FALSE(pref->IsDefaultValue());
  EXPECT_FALSE(pref->IsUserModifiable());
  EXPECT_FALSE(pref->IsUserControlled());
  EXPECT_TRUE(pref->IsManaged());
  EXPECT_FALSE(pref->IsRecommended());
  CheckPrefHasValue(pref, expected_value);
}

// Contains the testing details for a single pref affected by one or multiple
// policies. This is part of the data loaded from
// components/policy/test/data/policy_test_cases.json.
class PrefTestCase {
 public:
  PrefTestCase(const std::string& name, const base::Value::Dict& settings) {
    const base::Value* value = settings.Find("value");
    const base::Value* default_value = settings.Find("default_value");
    location_ = GetPrefLocation(settings);
    check_for_mandatory_ =
        settings.FindBool("check_for_mandatory").value_or(true);
    check_for_recommended_ =
        settings.FindBool("check_for_recommended").value_or(true);

    pref_ = name;
    if (value)
      value_ = value->Clone();
    if (default_value)
      default_value_ = default_value->Clone();

    if (value && default_value) {
      ADD_FAILURE()
          << "only one of |value| or |default_value| should be used for pref "
          << name;
    }
  }

  ~PrefTestCase() = default;
  PrefTestCase(const PrefTestCase& other) = delete;
  PrefTestCase& operator=(const PrefTestCase& other) = delete;

  const std::string& pref() const { return pref_; }

  const base::Value* value() const {
    if (value_.is_none())
      return nullptr;
    return &value_;
  }
  const base::Value* default_value() const {
    if (default_value_.is_none())
      return nullptr;
    return &default_value_;
  }

  PrefLocation location() const { return location_; }

  bool check_for_mandatory() const { return check_for_mandatory_; }
  bool check_for_recommended() const { return check_for_recommended_; }

 private:
  std::string pref_;
  PrefLocation location_;
  bool check_for_mandatory_;
  bool check_for_recommended_;

  // At most one of these will be set.
  base::Value value_;
  base::Value default_value_;
};

// Contains the testing details for a single pref affected by a policy. This is
// part of the data loaded from
// components/policy/test/data/policy_test_cases.json.
class PolicyPrefMappingTest {
 public:
  explicit PolicyPrefMappingTest(const base::Value::Dict& mapping) {
    const base::Value::Dict* policies = mapping.FindDict("policies");
    const base::Value::Dict* policies_settings =
        mapping.FindDict("policies_settings");
    const base::Value::Dict* prefs = mapping.FindDict("prefs");
    if (policies)
      policies_ = policies->Clone();
    if (policies_settings)
      policies_settings_ = policies_settings->Clone();
    if (prefs) {
      for (auto [name, setting] : *prefs) {
        if (!setting.is_dict()) {
          ADD_FAILURE() << "prefs item " << name << " is not dict";
          continue;
        }
        prefs_.push_back(
            std::make_unique<PrefTestCase>(name, setting.GetDict()));
      }
    }
    if (prefs_.empty()) {
      ADD_FAILURE() << "missing |prefs|";
    }
    const base::Value::List* required_buildflags =
        mapping.FindList("required_buildflags");
    if (required_buildflags) {
      for (const auto& required_buildflag : *required_buildflags) {
        required_buildflags_.push_back(required_buildflag.GetString());
      }
    }
  }
  ~PolicyPrefMappingTest() = default;
  PolicyPrefMappingTest(const PolicyPrefMappingTest& other) = delete;
  PolicyPrefMappingTest& operator=(const PolicyPrefMappingTest& other) = delete;

  const base::Value::Dict& policies() const { return policies_; }
  const base::Value::Dict& policies_settings() const {
    return policies_settings_;
  }

  const std::vector<std::unique_ptr<PrefTestCase>>& prefs() const {
    return prefs_;
  }

  const std::vector<std::string>& required_buildflags() const {
    return required_buildflags_;
  }

 private:
  base::Value::Dict policies_;
  base::Value::Dict policies_settings_;
  const std::string pref_;
  std::vector<std::unique_ptr<PrefTestCase>> prefs_;
  std::vector<std::string> required_buildflags_;
};

// Populates buildflags as strings that policy pref mapping test cases
// can depend on and implements a check if such a test case should run according
// to the buildflags.
bool CheckRequiredBuildFlagsSupported(const PolicyPrefMappingTest* test) {
  static base::NoDestructor<base::flat_set<std::string>> kBuildFlags([] {
    base::flat_set<std::string> flags;
#if BUILDFLAG(USE_CUPS)
    flags.insert("USE_CUPS");
#endif
    return flags;
  }());

  for (const auto& required_buildflag : test->required_buildflags()) {
    if (!kBuildFlags->contains(required_buildflag)) {
      return false;
    }
  }

  return true;
}

// Contains the testing details for a single policy. This is part of the data
// loaded from components/policy/test/data/policy_test_cases.json.
class PolicyTestCase {
 public:
  PolicyTestCase(const std::string& name, const base::Value::Dict& test_case)
      : name_(name) {
    is_official_only_ = test_case.FindBool("official_only").value_or(false);
    can_be_recommended_ =
        test_case.FindBool("can_be_recommended").value_or(false);
    has_reason_for_missing_test_ =
        test_case.FindString("reason_for_missing_test") != nullptr;

    const base::Value::List* os_list = test_case.FindList("os");
    if (os_list) {
      for (const auto& os : *os_list) {
        if (os.is_string())
          supported_os_.push_back(os.GetString());
      }
    }

    const base::Value::List* policy_pref_mapping_tests =
        test_case.FindList("policy_pref_mapping_tests");
    if (policy_pref_mapping_tests) {
      for (const auto& mapping : *policy_pref_mapping_tests) {
        if (mapping.is_dict()) {
          policy_pref_mapping_tests_.push_back(
              std::make_unique<PolicyPrefMappingTest>(mapping.GetDict()));
        }
      }
    }
  }

  ~PolicyTestCase() = default;
  PolicyTestCase(const PolicyTestCase& other) = delete;
  PolicyTestCase& operator=(const PolicyTestCase& other) = delete;

  const std::string& name() const { return name_; }

  bool is_official_only() const { return is_official_only_; }

  bool can_be_recommended() const { return can_be_recommended_; }

  bool has_reason_for_missing_test() const {
    return has_reason_for_missing_test_;
  }

  bool IsOsSupported() const {
#if BUILDFLAG(IS_ANDROID)
    const std::string os("android");
#elif BUILDFLAG(IS_CHROMEOS_ASH)
    const std::string os("chromeos_ash");
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
    const std::string os("chromeos_lacros");
#elif BUILDFLAG(IS_IOS)
    const std::string os("ios");
#elif BUILDFLAG(IS_LINUX)
    const std::string os("linux");
#elif BUILDFLAG(IS_MAC)
    const std::string os("mac");
#elif BUILDFLAG(IS_WIN)
    const std::string os("win");
#elif BUILDFLAG(IS_FUCHSIA)
    const std::string os("fuchsia");
#else
#error "Unknown platform"
#endif
    return base::Contains(supported_os_, os);
  }

  bool IsOsCovered() const {
#if BUILDFLAG(IS_CHROMEOS)
    return base::Contains(supported_os_, "chromeos_ash") ||
           base::Contains(supported_os_, "chromeos_lacros");
#else
    return IsOsSupported();
#endif
  }

  bool IsSupported() const {
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
    if (is_official_only())
      return false;
#endif
    return IsOsSupported();
  }

  const std::vector<std::unique_ptr<PolicyPrefMappingTest>>&
  policy_pref_mapping_tests() const {
    return policy_pref_mapping_tests_;
  }

  bool HasSupportedOs() const { return !supported_os_.empty(); }

 private:
  std::string name_;
  bool is_official_only_;
  bool can_be_recommended_;
  bool has_reason_for_missing_test_;
  std::vector<std::string> supported_os_;
  std::vector<std::unique_ptr<PolicyPrefMappingTest>>
      policy_pref_mapping_tests_;
};

// Parses all policy test cases and makes them available in a map.
class PolicyTestCases {
 public:
  typedef std::vector<std::unique_ptr<PolicyTestCase>> PolicyTestCaseVector;
  typedef std::map<std::string, PolicyTestCaseVector> PolicyTestCaseMap;
  typedef PolicyTestCaseMap::const_iterator iterator;

  explicit PolicyTestCases(const base::FilePath& test_case_path) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string json;
    if (!base::ReadFileToString(test_case_path, &json)) {
      ADD_FAILURE() << "Error reading: " << test_case_path;
      return;
    }
    auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(json);
    if (!parsed_json.has_value()) {
      ADD_FAILURE() << "Error parsing policy_test_cases.json: "
                    << parsed_json.error().message;
      return;
    }

    base::Value::Dict* dict = parsed_json->GetIfDict();
    if (!dict) {
      ADD_FAILURE()
          << "Error parsing policy_test_cases.json: Expected dictionary.";
      return;
    }
    for (auto it : *dict) {
      const std::string policy_name = GetPolicyName(it.first);
      if (policy_name == kInstructionKeyName)
        continue;
      auto policy_test_case =
          std::make_unique<PolicyTestCase>(it.first, it.second.GetDict());
      policy_test_cases_[policy_name].push_back(std::move(policy_test_case));
    }
  }

  ~PolicyTestCases() = default;
  PolicyTestCases(const PolicyTestCases& other) = delete;
  PolicyTestCases& operator=(const PolicyTestCases& other) = delete;

  const PolicyTestCaseVector* Get(const std::string& name) const {
    const iterator it = policy_test_cases_.find(name);
    return it == end() ? nullptr : &it->second;
  }

  const PolicyTestCaseMap& map() const { return policy_test_cases_; }
  iterator begin() const { return policy_test_cases_.begin(); }
  iterator end() const { return policy_test_cases_.end(); }

 private:
  PolicyTestCaseMap policy_test_cases_;
};

struct PolicySettings {
  PolicySource source = PolicySource::POLICY_SOURCE_CLOUD;
  PolicyScope scope = PolicyScope::POLICY_SCOPE_USER;
};

PolicySettings GetPolicySettings(const std::string& policy,
                                 const base::Value::Dict& policies_settings) {
  PolicySettings settings;
  const base::Value::Dict* settings_value =
      policies_settings.FindDictByDottedPath(policy);
  if (!settings_value)
    return settings;
  const std::string* source = settings_value->FindString("source");
  if (source) {
    if (*source == "enterprise_default")
      settings.source = POLICY_SOURCE_ENTERPRISE_DEFAULT;
    else if (*source == "command_line")
      settings.source = POLICY_SOURCE_COMMAND_LINE;
    else if (*source == "cloud")
      settings.source = POLICY_SOURCE_CLOUD;
    else if (*source == "active_directory")
      settings.source = POLICY_SOURCE_ACTIVE_DIRECTORY;
    else if (*source == "platform")
      settings.source = POLICY_SOURCE_PLATFORM;
    else if (*source == "merged")
      settings.source = POLICY_SOURCE_MERGED;
    else if (*source == "cloud_from_ash")
      settings.source = POLICY_SOURCE_CLOUD_FROM_ASH;
    else {
      NOTREACHED();
    }
  }

  const std::string* scope = settings_value->FindString("scope");
  if (scope) {
    if (*scope == "user")
      settings.scope = POLICY_SCOPE_USER;
    else if (*scope == "machine")
      settings.scope = POLICY_SCOPE_MACHINE;
  }

  return settings;
}

void SetProviderPolicy(MockConfigurationPolicyProvider* provider,
                       const base::Value::Dict& policies,
                       const base::Value::Dict& policies_settings,
                       PolicyLevel level) {
  PolicyMap policy_map;
#if BUILDFLAG(IS_CHROMEOS)
  SetEnterpriseUsersDefaults(&policy_map);
#endif  // BUILDFLAG(IS_CHROMEOS)
  for (auto it : policies) {
    const PolicyDetails* policy_details = GetChromePolicyDetails(it.first);
    const PolicySettings policy_settings =
        GetPolicySettings(it.first, policies_settings);
    ASSERT_TRUE(policy_details);
    policy_map.Set(
        it.first, level, policy_settings.scope, policy_settings.source,
        it.second.Clone(),
        policy_details->max_external_data_size
            ? std::make_unique<ExternalDataFetcher>(nullptr, it.first)
            : nullptr);
  }
  provider->UpdateChromePolicy(policy_map);
}

absl::optional<base::flat_set<std::string>> GetTestFilter() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          kPolicyToPrefMappingsFilterSwitch)) {
    return absl::nullopt;
  }

  std::string value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kPolicyToPrefMappingsFilterSwitch);
  auto list = base::SplitString(value, ":", base::TRIM_WHITESPACE,
                                base::SPLIT_WANT_NONEMPTY);
  if (list.empty())
    return absl::nullopt;

  return base::flat_set<std::string>(std::move(list));
}

}  // namespace

void VerifyAllPoliciesHaveATestCase(const base::FilePath& test_case_path) {
  // Verifies that all known policies have a test case in the JSON file.
  // This test fails when a policy is added to
  // components/policy/resources/policy_templates.json but a test case is not
  // added to components/policy/test/data/policy_test_cases.json.
  Schema chrome_schema = Schema::Wrap(GetChromeSchemaData());
  ASSERT_TRUE(chrome_schema.valid());

  PolicyTestCases policy_test_cases(test_case_path);
  for (Schema::Iterator it = chrome_schema.GetPropertiesIterator();
       !it.IsAtEnd(); it.Advance()) {
    auto policy = policy_test_cases.map().find(it.key());
    if (policy == policy_test_cases.map().end()) {
      ADD_FAILURE() << "Missing policy test case for: " << it.key();
      continue;
    }

    bool has_test_case_or_reason_for_this_os = false;
    bool has_reason_for_all_os = false;
    for (const auto& test_case : policy->second) {
      EXPECT_TRUE(test_case->has_reason_for_missing_test() ||
                  !test_case->policy_pref_mapping_tests().empty())
          << "Test case " << test_case->name()
          << " has empty list of test cases (policy_pref_mapping_tests). Add "
             "tests or use reason_for_missing_test.";

      if (test_case->HasSupportedOs()) {
        has_test_case_or_reason_for_this_os |= test_case->IsOsCovered();
      } else {
        has_reason_for_all_os |= test_case->has_reason_for_missing_test();
      }
    }
    EXPECT_TRUE(has_test_case_or_reason_for_this_os || has_reason_for_all_os)
        << "Policy " << policy->first
        << " should either provide a test case for all supported operating "
           "systems (see policy_templates.json) or provide a "
           "reason_for_missing_test.";
  }
}

// Verifies that policies make their corresponding preferences become managed,
// and that the user can't override that setting.
void VerifyPolicyToPrefMappings(const base::FilePath& test_case_path,
                                PrefService* local_state,
                                PrefService* user_prefs,
                                PrefService* signin_profile_prefs,
                                MockConfigurationPolicyProvider* provider,
                                PrefMappingChunkInfo* chunk_info) {
  Schema chrome_schema = Schema::Wrap(GetChromeSchemaData());
  ASSERT_TRUE(chrome_schema.valid());

  const PolicyTestCases test_cases(test_case_path);

  auto test_filter = GetTestFilter();

  for (const auto& policy : test_cases) {
    for (const auto& test_case : policy.second) {
      SCOPED_TRACE(::testing::Message()
                   << "Policy test case name: " << test_case->name());
      if (chunk_info != nullptr) {
        const size_t policy_name_hash = base::PersistentHash(policy.first);
        const size_t chunk_index = policy_name_hash % chunk_info->num_chunks;
        if (chunk_index != chunk_info->current_chunk)
          // Skip policy if test cases are chunked and the policy is not part of
          // the current chunk.
          continue;
      }

      if (test_filter.has_value() &&
          !base::Contains(test_filter.value(), test_case->name())) {
        // Skip policy based on the filter.
        continue;
      }

      if (!chrome_schema.GetKnownProperty(policy.first).valid() &&
          test_case->IsSupported()) {
        // Print warning message if a deprecated policy is still supported by
        // the test file.
        LOG(WARNING)
            << "Policy " << policy.first
            << " is marked as supported on this OS but does not exist in the "
            << "Chrome policy schema.";
        continue;
      }

      if (!test_case->IsSupported() ||
          test_case->has_reason_for_missing_test()) {
        continue;
      }

      for (size_t i = 0; i < test_case->policy_pref_mapping_tests().size();
           ++i) {
        const auto& pref_mapping = test_case->policy_pref_mapping_tests()[i];
        SCOPED_TRACE(::testing::Message() << "Mapping test index " << i);

        EXPECT_FALSE(pref_mapping->prefs().empty())
            << "Test #" << i << " for " << test_case->name()
            << " is missing pref values to check for";

        if (!CheckRequiredBuildFlagsSupported(pref_mapping.get())) {
          LOG(INFO) << "Test #" << i << " for " << test_case->name()
                    << " skipped due to buildflags";
          continue;
        }

        for (const auto& pref_case : pref_mapping->prefs()) {
          SCOPED_TRACE(::testing::Message() << "Pref: " << pref_case->pref());
          PrefService* prefs =
              GetPrefServiceForLocation(pref_case->location(), local_state,
                                        user_prefs, signin_profile_prefs);
          // Skip preference mapping if required PrefService was not provided.
          if (!prefs)
            continue;

          const bool check_recommended = test_case->can_be_recommended() &&
                                         pref_case->check_for_recommended();
          const bool check_mandatory = pref_case->check_for_mandatory();

          EXPECT_TRUE(check_recommended || check_mandatory)
              << "pref has to be checked for recommended and/or mandatory "
                 "values";

          // The preference must have been registered.
          const PrefService::Preference* pref =
              prefs->FindPreference(pref_case->pref());
          ASSERT_TRUE(pref)
              << "Pref " << pref_case->pref() << " not registered";

          provider->UpdateChromePolicy(PolicyMap());
          prefs->ClearPref(pref_case->pref());
          CheckPrefHasDefaultValue(pref);

          const base::Value::Dict& policies = pref_mapping->policies();

          const base::Value* expected_value = pref_case->value();
          bool expect_value_to_be_default = false;
          if (!expected_value && pref_case->default_value()) {
            expected_value = pref_case->default_value();
            expect_value_to_be_default = true;
          }
          if (!expected_value && policies.size() == 1) {
            // If no value/default value is specified, fall back to the policy
            // value (if only one policy is set).
            expected_value = &policies.begin()->second;
            expect_value_to_be_default = false;
          }
          ASSERT_TRUE(expected_value);

          if (check_recommended) {
            SCOPED_TRACE(::testing::Message() << "checking recommended policy");

            ASSERT_NO_FATAL_FAILURE(SetProviderPolicy(
                provider, policies, pref_mapping->policies_settings(),
                POLICY_LEVEL_RECOMMENDED));
            if (expect_value_to_be_default) {
              CheckPrefHasDefaultValue(pref, expected_value);
            } else {
              CheckPrefHasRecommendedValue(pref, expected_value);
            }
          }

          if (check_mandatory) {
            SCOPED_TRACE(::testing::Message() << "checking mandatory policy");
            ASSERT_NO_FATAL_FAILURE(SetProviderPolicy(
                provider, policies, pref_mapping->policies_settings(),
                POLICY_LEVEL_MANDATORY));
            if (expect_value_to_be_default) {
              CheckPrefHasDefaultValue(pref, expected_value);
            } else {
              CheckPrefHasMandatoryValue(pref, expected_value);
            }
          }
        }
      }
    }
  }
}

}  // namespace policy
