// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/variations/study_filtering.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <array>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "client_filterable_state.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/processed_study.h"
#include "components/variations/variations_layers.h"
#include "components/variations/variations_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

namespace {

// Converts |time| to Study proto format.
int64_t TimeToProtoTime(const base::Time& time) {
  return (time - base::Time::UnixEpoch()).InSeconds();
}

// Adds an experiment to |study| with the specified |name| and |probability|.
Study::Experiment* AddExperiment(const std::string& name,
                                 int probability,
                                 Study* study) {
  Study::Experiment* experiment = study->add_experiment();
  experiment->set_name(name);
  experiment->set_probability_weight(probability);
  return experiment;
}

std::vector<std::string> SplitFilterString(const std::string& input) {
  return base::SplitString(input, ",", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_ALL);
}

ClientFilterableState ClientFilterableStateForGoogleGroups(
    const base::flat_set<uint64_t> google_groups) {
  return ClientFilterableState(
      base::BindOnce([] { return false; }),
      base::BindLambdaForTesting([=]() { return google_groups; }));
}

}  // namespace

TEST(VariationsStudyFilteringTest, CheckStudyChannel) {
  constexpr auto channels = std::to_array<Study::Channel>(
      {Study::CANARY, Study::DEV, Study::BETA, Study::STABLE});
  std::array<bool, channels.size()> channel_added = {false};

  Study::Filter filter;

  // Check in the forwarded order. The loop cond is <= channels.size()
  // instead of < so that the result of adding the last channel gets checked.
  for (size_t i = 0; i <= channels.size(); ++i) {
    for (size_t j = 0; j < channels.size(); ++j) {
      const bool expected = channel_added[j] || filter.channel_size() == 0;
      const bool result = internal::CheckStudyChannel(filter, channels[j]);
      EXPECT_EQ(expected, result) << "Case " << i << "," << j << " failed!";
    }

    if (i < channels.size()) {
      filter.add_channel(channels[i]);
      channel_added[i] = true;
    }
  }

  // Do the same check in the reverse order.
  filter.clear_channel();
  std::ranges::fill(channel_added, false);
  for (size_t i = 0; i <= channels.size(); ++i) {
    for (size_t j = 0; j < channels.size(); ++j) {
      const bool expected = channel_added[j] || filter.channel_size() == 0;
      const bool result = internal::CheckStudyChannel(filter, channels[j]);
      EXPECT_EQ(expected, result) << "Case " << i << "," << j << " failed!";
    }

    if (i < channels.size()) {
      const int index = channels.size() - i - 1;
      filter.add_channel(channels[index]);
      channel_added[index] = true;
    }
  }
}

TEST(VariationsStudyFilteringTest, CheckStudyFormFactor) {
  constexpr auto form_factors = std::to_array<Study::FormFactor>(
      {Study::DESKTOP, Study::PHONE, Study::TABLET, Study::KIOSK,
       Study::MEET_DEVICE, Study::TV, Study::AUTOMOTIVE, Study::FOLDABLE});

  ASSERT_EQ(Study::FormFactor_ARRAYSIZE, static_cast<int>(form_factors.size()));

  std::array<bool, form_factors.size()> form_factor_added = {false};
  Study::Filter filter;

  for (size_t i = 0; i <= form_factors.size(); ++i) {
    for (size_t j = 0; j < form_factors.size(); ++j) {
      const bool expected = form_factor_added[j] ||
                            filter.form_factor_size() == 0;
      const bool result = internal::CheckStudyFormFactor(filter,
                                                         form_factors[j]);
      EXPECT_EQ(expected, result) << "form_factor: case " << i << "," << j
                                  << " failed!";
    }

    if (i < form_factors.size()) {
      filter.add_form_factor(form_factors[i]);
      form_factor_added[i] = true;
    }
  }

  // Do the same check in the reverse order.
  filter.clear_form_factor();
  std::ranges::fill(form_factor_added, false);
  for (size_t i = 0; i <= form_factors.size(); ++i) {
    for (size_t j = 0; j < form_factors.size(); ++j) {
      const bool expected = form_factor_added[j] ||
                            filter.form_factor_size() == 0;
      const bool result = internal::CheckStudyFormFactor(filter,
                                                         form_factors[j]);
      EXPECT_EQ(expected, result) << "form_factor: case " << i << "," << j
                                  << " failed!";
    }

    if (i < form_factors.size()) {
      const int index = form_factors.size() - i - 1;
      filter.add_form_factor(form_factors[index]);
      form_factor_added[index] = true;
    }
  }

  // Test exclude_form_factors, forward order.
  filter.clear_form_factor();
  std::array<bool, form_factors.size()> form_factor_excluded = {false};
  for (size_t i = 0; i <= form_factors.size(); ++i) {
    for (size_t j = 0; j < form_factors.size(); ++j) {
      const bool expected = filter.exclude_form_factor_size() == 0 ||
                            !form_factor_excluded[j];
      const bool result = internal::CheckStudyFormFactor(filter,
                                                         form_factors[j]);
      EXPECT_EQ(expected, result) << "exclude_form_factor: case " << i << ","
                                  << j << " failed!";
    }

    if (i < form_factors.size()) {
      filter.add_exclude_form_factor(form_factors[i]);
      form_factor_excluded[i] = true;
    }
  }

  // Test exclude_form_factors, reverse order.
  filter.clear_exclude_form_factor();
  std::ranges::fill(form_factor_excluded, false);
  for (size_t i = 0; i <= form_factors.size(); ++i) {
    for (size_t j = 0; j < form_factors.size(); ++j) {
      const bool expected = filter.exclude_form_factor_size() == 0 ||
                            !form_factor_excluded[j];
      const bool result = internal::CheckStudyFormFactor(filter,
                                                         form_factors[j]);
      EXPECT_EQ(expected, result) << "exclude_form_factor: case " << i << ","
                                  << j << " failed!";
    }

    if (i < form_factors.size()) {
      const int index = form_factors.size() - i - 1;
      filter.add_exclude_form_factor(form_factors[index]);
      form_factor_excluded[index] = true;
    }
  }
}

TEST(VariationsStudyFilteringTest, CheckStudyLocale) {
  struct {
    const char* filter_locales;
    const char* exclude_locales;
    bool en_us_result;
    bool en_ca_result;
    bool fr_result;
  } test_cases[] = {
      {"en-US", "", true, false, false},
      // Tests that locale overrides exclude_locale, when both are given. This
      // should not occur in practice though.
      {"en-US", "en-US", true, false, false},
      {"en-US,en-CA,fr", "", true, true, true},
      {"en-US,en-CA,en-GB", "", true, true, false},
      {"en-GB,en-CA,en-US", "", true, true, false},
      {"ja,kr,vi", "", false, false, false},
      {"fr-CA", "", false, false, false},
      {"", "", true, true, true},
      {"", "en-US", false, true, true},
      {"", "en-US,en-CA,fr", false, false, false},
      {"", "en-US,en-CA,en-GB", false, false, true},
      {"", "en-GB,en-CA,en-US", false, false, true},
      {"", "ja,kr,vi", true, true, true},
      {"", "fr-CA", true, true, true},
  };

  for (const auto& test : test_cases) {
    Study::Filter filter;
    for (const std::string& locale : SplitFilterString(test.filter_locales))
      filter.add_locale(locale);
    for (const std::string& locale : SplitFilterString(test.exclude_locales))
      filter.add_exclude_locale(locale);

    EXPECT_EQ(test.en_us_result, internal::CheckStudyLocale(filter, "en-US"));
    EXPECT_EQ(test.en_ca_result, internal::CheckStudyLocale(filter, "en-CA"));
    EXPECT_EQ(test.fr_result, internal::CheckStudyLocale(filter, "fr"));
  }
}

TEST(VariationsStudyFilteringTest, CheckStudyPlatform) {
  constexpr auto platforms = std::to_array<Study::Platform>(
      {Study::PLATFORM_WINDOWS, Study::PLATFORM_MAC, Study::PLATFORM_LINUX,
       Study::PLATFORM_CHROMEOS, Study::PLATFORM_CHROMEOS_LACROS,
       Study::PLATFORM_ANDROID, Study::PLATFORM_IOS,
       Study::PLATFORM_ANDROID_WEBLAYER, Study::PLATFORM_FUCHSIA,
       Study::PLATFORM_ANDROID_WEBVIEW});
  static_assert(platforms.size() == Study::Platform_ARRAYSIZE,
                "|platforms| must include all platforms.");
  std::array<bool, platforms.size()> platform_added = {false};

  Study::Filter filter;

  // Check in the forwarded order. The loop cond is <= platforms.size()
  // instead of < so that the result of adding the last platform gets checked.
  for (size_t i = 0; i <= platforms.size(); ++i) {
    for (size_t j = 0; j < platforms.size(); ++j) {
      const bool expected = platform_added[j];
      const bool result = internal::CheckStudyPlatform(filter, platforms[j]);
      EXPECT_EQ(expected, result) << "Case " << i << "," << j << " failed!";
    }

    if (i < platforms.size()) {
      filter.add_platform(platforms[i]);
      platform_added[i] = true;
    }
  }

  // Do the same check in the reverse order.
  filter.clear_platform();
  std::ranges::fill(platform_added, false);
  for (size_t i = 0; i <= platforms.size(); ++i) {
    for (size_t j = 0; j < platforms.size(); ++j) {
      const bool expected = platform_added[j];
      const bool result = internal::CheckStudyPlatform(filter, platforms[j]);
      EXPECT_EQ(expected, result) << "Case " << i << "," << j << " failed!";
    }

    if (i < platforms.size()) {
      const int index = platforms.size() - i - 1;
      filter.add_platform(platforms[index]);
      platform_added[index] = true;
    }
  }
}

TEST(VariationsStudyFilteringTest, CheckStudyLowEndDevice) {
  Study::Filter filter;

  // Check that if the filter is not set, study applies to either low end value.
  EXPECT_TRUE(internal::CheckStudyLowEndDevice(filter, true));
  EXPECT_TRUE(internal::CheckStudyLowEndDevice(filter, false));

  filter.set_is_low_end_device(true);
  EXPECT_TRUE(internal::CheckStudyLowEndDevice(filter, true));
  EXPECT_FALSE(internal::CheckStudyLowEndDevice(filter, false));

  filter.set_is_low_end_device(false);
  EXPECT_FALSE(internal::CheckStudyLowEndDevice(filter, true));
  EXPECT_TRUE(internal::CheckStudyLowEndDevice(filter, false));
}

TEST(VariationsStudyFilteringTest, CheckStudyEnterprise) {
  Study::Filter filter;
  ClientFilterableState client_non_enterprise(
      base::BindOnce([] { return false; }),
      base::BindOnce([] { return base::flat_set<uint64_t>(); }));
  ClientFilterableState client_enterprise(
      base::BindOnce([] { return true; }),
      base::BindOnce([] { return base::flat_set<uint64_t>(); }));

  // Check that if the filter is not set, study applies to both enterprise and
  // non-enterprise clients.
  EXPECT_TRUE(internal::CheckStudyEnterprise(filter, client_enterprise));
  EXPECT_TRUE(internal::CheckStudyEnterprise(filter, client_non_enterprise));

  filter.set_is_enterprise(true);
  EXPECT_TRUE(internal::CheckStudyEnterprise(filter, client_enterprise));
  EXPECT_FALSE(internal::CheckStudyEnterprise(filter, client_non_enterprise));

  filter.set_is_enterprise(false);
  EXPECT_FALSE(internal::CheckStudyEnterprise(filter, client_enterprise));
  EXPECT_TRUE(internal::CheckStudyEnterprise(filter, client_non_enterprise));
}

TEST(VariationsStudyFilteringTest, CheckStudyPolicyRestriction) {
  Study::Filter filter;

  // Check that if the filter is not set, study applies to clients with no
  // restrictive policy.
  EXPECT_TRUE(internal::CheckStudyPolicyRestriction(
      filter, RestrictionPolicy::NO_RESTRICTIONS));
  EXPECT_FALSE(internal::CheckStudyPolicyRestriction(
      filter, RestrictionPolicy::CRITICAL_ONLY));
  EXPECT_FALSE(
      internal::CheckStudyPolicyRestriction(filter, RestrictionPolicy::ALL));

  // Explicitly set to none filter should be the same as no filter.
  filter.set_policy_restriction(Study::NONE);
  EXPECT_TRUE(internal::CheckStudyPolicyRestriction(
      filter, RestrictionPolicy::NO_RESTRICTIONS));
  EXPECT_FALSE(internal::CheckStudyPolicyRestriction(
      filter, RestrictionPolicy::CRITICAL_ONLY));
  EXPECT_FALSE(
      internal::CheckStudyPolicyRestriction(filter, RestrictionPolicy::ALL));

  // If the filter is set to CRITICAL then apply it to all clients that do not
  // disable all experiements.
  filter.set_policy_restriction(Study::CRITICAL);
  EXPECT_TRUE(internal::CheckStudyPolicyRestriction(
      filter, RestrictionPolicy::NO_RESTRICTIONS));
  EXPECT_TRUE(internal::CheckStudyPolicyRestriction(
      filter, RestrictionPolicy::CRITICAL_ONLY));
  EXPECT_FALSE(
      internal::CheckStudyPolicyRestriction(filter, RestrictionPolicy::ALL));

  // If the filter is set to CRITICAL_ONLY then apply it only to clients that
  // have requested critical studies but not to clients with no or full
  // restrictions.
  filter.set_policy_restriction(Study::CRITICAL_ONLY);
  EXPECT_FALSE(internal::CheckStudyPolicyRestriction(
      filter, RestrictionPolicy::NO_RESTRICTIONS));
  EXPECT_TRUE(internal::CheckStudyPolicyRestriction(
      filter, RestrictionPolicy::CRITICAL_ONLY));
  EXPECT_FALSE(
      internal::CheckStudyPolicyRestriction(filter, RestrictionPolicy::ALL));
}

TEST(VariationsStudyFilteringTest, CheckStudyStartDate) {
  const base::Time now = base::Time::Now();
  const base::TimeDelta delta = base::Hours(1);
  const struct {
    const base::Time start_date;
    bool expected_result;
  } start_test_cases_raw[] = {
      {now - delta, true},
      // Note, the proto start_date is truncated to seconds, but the reference
      // date isn't.
      {now, true},
      {now + delta, false},
  };
  const auto start_test_cases = base::span(start_test_cases_raw);

  Study::Filter filter;

  // Start date not set should result in true.
  EXPECT_TRUE(internal::CheckStudyStartDate(filter, now));

  for (size_t i = 0; i < start_test_cases.size(); ++i) {
    filter.set_start_date(TimeToProtoTime(start_test_cases[i].start_date));
    const bool result = internal::CheckStudyStartDate(filter, now);
    EXPECT_EQ(start_test_cases[i].expected_result, result)
        << "Case " << i << " failed!";
  }
}

TEST(VariationsStudyFilteringTest, CheckStudyEndDate) {
  const base::Time now = base::Time::Now();
  const base::TimeDelta delta = base::Hours(1);
  const struct {
    const base::Time end_date;
    bool expected_result;
  } start_test_cases_raw[] = {
      {now - delta, false},
      {now + delta, true},
  };
  const auto start_test_cases = base::span(start_test_cases_raw);

  Study::Filter filter;

  // End date not set should result in true.
  EXPECT_TRUE(internal::CheckStudyEndDate(filter, now));

  for (size_t i = 0; i < start_test_cases.size(); ++i) {
    filter.set_end_date(TimeToProtoTime(start_test_cases[i].end_date));
    const bool result = internal::CheckStudyEndDate(filter, now);
    EXPECT_EQ(start_test_cases[i].expected_result, result) << "Case " << i
                                                           << " failed!";
  }
}

TEST(VariationsStudyFilteringTest, CheckStudyOSVersion) {
  const struct {
    const char* min_os_version;
    const char* os_version;
    bool expected_result;
  } min_test_cases_raw[] = {
      {"1.2.2", "1.2.3", true},
      {"1.2.3", "1.2.3", true},
      {"1.2.4", "1.2.3", false},
      {"1.3.2", "1.2.3", false},
      {"2.1.2", "1.2.3", false},
      {"0.3.4", "1.2.3", true},
      // Wildcards.
      {"1.*", "1.2.3", true},
      {"1.2.*", "1.2.3", true},
      {"1.2.3.*", "1.2.3", true},
      {"1.2.4.*", "1.2.3", false},
      {"2.*", "1.2.3", false},
      {"0.3.*", "1.2.3", true},
  };
  const auto min_test_cases = base::span(min_test_cases_raw);

  const struct {
    const char* max_os_version;
    const char* os_version;
    bool expected_result;
  } max_test_cases_raw[] = {
      {"1.2.2", "1.2.3", false},
      {"1.2.3", "1.2.3", true},
      {"1.2.4", "1.2.3", true},
      {"2.1.1", "1.2.3", true},
      {"2.1.1", "2.3.4", false},
      // Wildcards
      {"2.1.*", "2.3.4", false},
      {"2.*", "2.3.4", true},
      {"2.3.*", "2.3.4", true},
      {"2.3.4.*", "2.3.4", true},
      {"2.3.4.0.*", "2.3.4", true},
      {"2.4.*", "2.3.4", true},
      {"1.3.*", "2.3.4", false},
      {"1.*", "2.3.4", false},
  };
  const auto max_test_cases = base::span(max_test_cases_raw);

  Study::Filter filter;

  // Min/max version not set should result in true.
  EXPECT_TRUE(internal::CheckStudyOSVersion(filter, base::Version("1.2.3")));

  for (size_t i = 0; i < min_test_cases.size(); ++i) {
    filter.set_min_os_version(min_test_cases[i].min_os_version);
    const bool result = internal::CheckStudyOSVersion(
        filter, base::Version(min_test_cases[i].os_version));
    EXPECT_EQ(min_test_cases[i].expected_result, result)
        << "Min OS version case " << i << " failed!";
  }
  filter.clear_min_os_version();

  for (size_t i = 0; i < max_test_cases.size(); ++i) {
    filter.set_max_os_version(max_test_cases[i].max_os_version);
    const bool result = internal::CheckStudyOSVersion(
        filter, base::Version(max_test_cases[i].os_version));
    EXPECT_EQ(max_test_cases[i].expected_result, result)
        << "Max OS version case " << i << " failed!";
  }

  // Check intersection semantics.
  for (size_t i = 0; i < min_test_cases.size(); ++i) {
    for (size_t j = 0; j < max_test_cases.size(); ++j) {
      filter.set_min_os_version(min_test_cases[i].min_os_version);
      filter.set_max_os_version(max_test_cases[j].max_os_version);

      if (!min_test_cases[i].expected_result) {
        const bool result = internal::CheckStudyOSVersion(
            filter, base::Version(min_test_cases[i].os_version));
        EXPECT_FALSE(result) << "Case " << i << "," << j << " failed!";
      }

      if (!max_test_cases[j].expected_result) {
        const bool result = internal::CheckStudyOSVersion(
            filter, base::Version(max_test_cases[j].os_version));
        EXPECT_FALSE(result) << "Case " << i << "," << j << " failed!";
      }
    }
  }
}

TEST(VariationsStudyFilteringTest, CheckStudyMalformedOSVersion) {
  Study::Filter filter;

  filter.set_min_os_version("1.2.0");
  EXPECT_FALSE(internal::CheckStudyOSVersion(filter, base::Version("1.2.a")));
  EXPECT_TRUE(internal::CheckStudyOSVersion(filter, base::Version("1.2.3")));
  filter.clear_min_os_version();

  filter.set_max_os_version("1.2.3");
  EXPECT_FALSE(internal::CheckStudyOSVersion(filter, base::Version("1.2.a")));
  EXPECT_TRUE(internal::CheckStudyOSVersion(filter, base::Version("1.2.3")));
}

TEST(VariationsStudyFilteringTest, CheckStudyVersion) {
  const struct {
    const char* min_version;
    const char* version;
    bool expected_result;
  } min_test_cases_raw[] = {
      {"1.2.2", "1.2.3", true},
      {"1.2.3", "1.2.3", true},
      {"1.2.4", "1.2.3", false},
      {"1.3.2", "1.2.3", false},
      {"2.1.2", "1.2.3", false},
      {"0.3.4", "1.2.3", true},
      // Wildcards.
      {"1.*", "1.2.3", true},
      {"1.2.*", "1.2.3", true},
      {"1.2.3.*", "1.2.3", true},
      {"1.2.4.*", "1.2.3", false},
      {"2.*", "1.2.3", false},
      {"0.3.*", "1.2.3", true},
  };
  const auto min_test_cases = base::span(min_test_cases_raw);

  const struct {
    const char* max_version;
    const char* version;
    bool expected_result;
  } max_test_cases_raw[] = {
      {"1.2.2", "1.2.3", false},
      {"1.2.3", "1.2.3", true},
      {"1.2.4", "1.2.3", true},
      {"2.1.1", "1.2.3", true},
      {"2.1.1", "2.3.4", false},
      // Wildcards
      {"2.1.*", "2.3.4", false},
      {"2.*", "2.3.4", true},
      {"2.3.*", "2.3.4", true},
      {"2.3.4.*", "2.3.4", true},
      {"2.3.4.0.*", "2.3.4", true},
      {"2.4.*", "2.3.4", true},
      {"1.3.*", "2.3.4", false},
      {"1.*", "2.3.4", false},
  };
  const auto max_test_cases = base::span(max_test_cases_raw);

  Study::Filter filter;

  // Min/max version not set should result in true.
  EXPECT_TRUE(internal::CheckStudyVersion(filter, base::Version("1.2.3")));

  for (size_t i = 0; i < min_test_cases.size(); ++i) {
    filter.set_min_version(min_test_cases[i].min_version);
    const bool result = internal::CheckStudyVersion(
        filter, base::Version(min_test_cases[i].version));
    EXPECT_EQ(min_test_cases[i].expected_result, result) <<
        "Min. version case " << i << " failed!";
  }
  filter.clear_min_version();

  for (size_t i = 0; i < max_test_cases.size(); ++i) {
    filter.set_max_version(max_test_cases[i].max_version);
    const bool result = internal::CheckStudyVersion(
        filter, base::Version(max_test_cases[i].version));
    EXPECT_EQ(max_test_cases[i].expected_result, result) <<
        "Max version case " << i << " failed!";
  }

  // Check intersection semantics.
  for (size_t i = 0; i < min_test_cases.size(); ++i) {
    for (size_t j = 0; j < max_test_cases.size(); ++j) {
      filter.set_min_version(min_test_cases[i].min_version);
      filter.set_max_version(max_test_cases[j].max_version);

      if (!min_test_cases[i].expected_result) {
        const bool result = internal::CheckStudyVersion(
            filter, base::Version(min_test_cases[i].version));
        EXPECT_FALSE(result) << "Case " << i << "," << j << " failed!";
      }

      if (!max_test_cases[j].expected_result) {
        const bool result = internal::CheckStudyVersion(
            filter, base::Version(max_test_cases[j].version));
        EXPECT_FALSE(result) << "Case " << i << "," << j << " failed!";
      }
    }
  }
}

TEST(VariationsStudyFilteringTest, CheckStudyHardwareClass) {
  struct {
    const char* hardware_class;
    const char* exclude_hardware_class;
    const char* actual_hardware_class;
    bool expected_result;
  } test_cases[] = {
      // Neither filtered nor excluded set:
      // True since empty is always a match.
      {"", "", "fancy INTEL pear device", true},
      {"", "", "", true},

      // Filtered set:
      {"apple,pear,orange", "", "apple", true},
      {"apple,pear,orange", "", "aPPle", true},
      {"apple,pear,orange", "", "fancy INTEL pear device", false},
      {"apple,pear,orange", "", "fancy INTEL GRAPE device", false},
      // Somehow tagged as both, but still valid.
      {"apple,pear,orange", "", "fancy INTEL pear GRAPE device", false},
      // Substring, which should not match.
      {"apple,pear,orange", "", "fancy INTEL SNapple device", false},
      // Empty, which is what would happen for non ChromeOS platforms.
      {"apple,pear,orange", "", "", false},

      // Excluded set:
      {"", "apple,pear,orange", "apple", false},
      {"", "apple,pear,orange", "fancy INTEL pear device", true},
      {"", "apple,pear,orange", "fancy INTEL GRAPE device", true},
      // Empty.
      {"", "apple,pear,orange", "", true},

      // Not testing when both are set as it should never occur and should be
      // considered undefined.
  };

  for (const auto& test : test_cases) {
    Study::Filter filter;
    for (const auto& hw_class : SplitFilterString(test.hardware_class))
      filter.add_hardware_class(hw_class);
    for (const auto& hw_class :
         SplitFilterString(test.exclude_hardware_class)) {
      filter.add_exclude_hardware_class(hw_class);
    }

    EXPECT_EQ(test.expected_result, internal::CheckStudyHardwareClass(
                                        filter, test.actual_hardware_class))
        << "hardware_class=" << test.hardware_class << " "
        << "exclude_hardware_class=" << test.exclude_hardware_class << " "
        << "actual_hardware_class=" << test.actual_hardware_class;
  }
}

TEST(VariationsStudyFilteringTest, CheckStudyCountry) {
  struct {
    const char* country;
    const char* exclude_country;
    const char* actual_country;
    bool expected_result;
  } test_cases[] = {
      // Neither filtered nor excluded set:
      // True since empty is always a match.
      {"", "", "us", true},
      {"", "", "", true},

      // Filtered set:
      {"us", "", "us", true},
      {"br,ca,us", "", "us", true},
      {"br,ca,us", "", "in", false},
      // Empty, which is what would happen if no country was returned from the
      // server.
      {"br,ca,us", "", "", false},

      // Excluded set:
      {"", "us", "us", false},
      {"", "br,ca,us", "us", false},
      {"", "br,ca,us", "in", true},
      // Empty, which is what would happen if no country was returned from the
      // server.
      {"", "br,ca,us", "", true},

      // Not testing when both are set as it should never occur and should be
      // considered undefined.
  };

  for (const auto& test : test_cases) {
    Study::Filter filter;
    for (const std::string& country : SplitFilterString(test.country))
      filter.add_country(country);
    for (const std::string& country : SplitFilterString(test.exclude_country))
      filter.add_exclude_country(country);

    EXPECT_EQ(test.expected_result,
              internal::CheckStudyCountry(filter, test.actual_country));
  }
}

TEST(VariationsStudyFilteringTest, CheckStudyGoogleGroupFilterNotSet) {
  Study::Filter filter;

  // Check that if the filter is not set, the study always applies.
  EXPECT_TRUE(internal::CheckStudyGoogleGroup(
      filter,
      ClientFilterableStateForGoogleGroups(base::flat_set<uint64_t>())));
  EXPECT_TRUE(internal::CheckStudyGoogleGroup(
      filter,
      ClientFilterableStateForGoogleGroups(base::flat_set<uint64_t>({1}))));
}

TEST(VariationsStudyFilteringTest, CheckStudyGoogleGroupFilterSet) {
  Study::Filter filter;

  // Check that if a google_group filter is set, then only members of that group
  // match.
  filter.add_google_group(1);
  filter.add_google_group(2);
  EXPECT_FALSE(internal::CheckStudyGoogleGroup(
      filter,
      ClientFilterableStateForGoogleGroups(base::flat_set<uint64_t>())));
  EXPECT_TRUE(internal::CheckStudyGoogleGroup(
      filter,
      ClientFilterableStateForGoogleGroups(base::flat_set<uint64_t>({1}))));
  EXPECT_FALSE(internal::CheckStudyGoogleGroup(
      filter,
      ClientFilterableStateForGoogleGroups(base::flat_set<uint64_t>({3}))));
  EXPECT_TRUE(internal::CheckStudyGoogleGroup(
      filter,
      ClientFilterableStateForGoogleGroups(base::flat_set<uint64_t>({1, 3}))));
}

TEST(VariationsStudyFilteringTest, CheckStudyExcludeGoogleGroupFilterSet) {
  Study::Filter filter;

  // Check that if an exclude_google_group filter is set, then only non-members
  // of that group match.
  filter.add_exclude_google_group(1);
  filter.add_exclude_google_group(2);
  EXPECT_TRUE(internal::CheckStudyGoogleGroup(
      filter,
      ClientFilterableStateForGoogleGroups(base::flat_set<uint64_t>())));
  EXPECT_FALSE(internal::CheckStudyGoogleGroup(
      filter,
      ClientFilterableStateForGoogleGroups(base::flat_set<uint64_t>({1}))));
  EXPECT_TRUE(internal::CheckStudyGoogleGroup(
      filter,
      ClientFilterableStateForGoogleGroups(base::flat_set<uint64_t>({3}))));
  EXPECT_FALSE(internal::CheckStudyGoogleGroup(
      filter,
      ClientFilterableStateForGoogleGroups(base::flat_set<uint64_t>({1, 3}))));
}

TEST(VariationsStudyFilteringTest, CheckStudyBothGoogleGroupFiltersSet) {
  Study::Filter filter;

  // Check that both google_group and exclude_google_group filter is set, the
  // study is filtered out.
  filter.add_google_group(1);
  filter.add_exclude_google_group(2);
  EXPECT_FALSE(internal::CheckStudyGoogleGroup(
      filter,
      ClientFilterableStateForGoogleGroups(base::flat_set<uint64_t>())));
  EXPECT_FALSE(internal::CheckStudyGoogleGroup(
      filter,
      ClientFilterableStateForGoogleGroups(base::flat_set<uint64_t>({1}))));
  EXPECT_FALSE(internal::CheckStudyGoogleGroup(
      filter,
      ClientFilterableStateForGoogleGroups(base::flat_set<uint64_t>({2}))));
  EXPECT_FALSE(internal::CheckStudyGoogleGroup(
      filter,
      ClientFilterableStateForGoogleGroups(base::flat_set<uint64_t>({1, 2}))));
}

TEST(VariationsStudyFilteringTest, FilterAndValidateStudies) {
  const std::string kTrial1Name = "A";
  const std::string kGroup1Name = "Group1";
  const std::string kTrial3Name = "B";

  VariationsSeed seed;
  Study* study1 = seed.add_study();
  study1->set_name(kTrial1Name);
  study1->set_default_experiment_name("Default");
  AddExperiment(kGroup1Name, 100, study1);
  AddExperiment("Default", 0, study1);

  Study* study2 = seed.add_study();
  *study2 = *study1;
  study2->mutable_experiment(0)->set_name("Bam");
  ASSERT_EQ(seed.study(0).name(), seed.study(1).name());

  Study* study3 = seed.add_study();
  study3->set_name(kTrial3Name);
  study3->set_default_experiment_name("Default");
  AddExperiment("A", 10, study3);
  AddExperiment("Default", 25, study3);

  auto client_state = CreateDummyClientFilterableState();
  client_state->locale = "en-CA";
  client_state->reference_date = base::Time::Now();
  client_state->version = base::Version("20.0.0.0");
  client_state->channel = Study::STABLE;
  client_state->form_factor = Study::DESKTOP;
  client_state->platform = Study::PLATFORM_ANDROID;

  std::vector<ProcessedStudy> processed_studies =
      FilterAndValidateStudies(seed, *client_state, VariationsLayers());

  // Check that only the first kTrial1Name study was kept.
  ASSERT_EQ(2U, processed_studies.size());
  EXPECT_EQ(kTrial1Name, processed_studies[0].study()->name());
  EXPECT_EQ(kGroup1Name, processed_studies[0].study()->experiment(0).name());
  EXPECT_EQ(kTrial3Name, processed_studies[1].study()->name());
}

TEST(VariationsStudyFilteringTest, FilterAndValidateStudiesWithBadFilters) {
  constexpr auto versions = std::to_array<const char*>(
      {"invalid", "1.invalid.0", "0.invalid.0", "\001\000\000\003"});
  VariationsSeed seed;

  Study baseStudy;
  baseStudy.set_default_experiment_name("Default");
  AddExperiment("Default", 100, &baseStudy);
  baseStudy.mutable_filter()->add_platform(Study::PLATFORM_ANDROID);

  // Add studies with invalid min_versions.
  for (size_t i = 0; i < std::size(versions); ++i) {
    Study* study = seed.add_study();
    *study = baseStudy;
    study->set_name(
        base::StrCat({"min_version_study_", base::NumberToString(i)}));
    study->mutable_filter()->set_min_version(versions[i]);
  }

  // Add studies with invalid max_versions.
  for (size_t i = 0; i < std::size(versions); ++i) {
    Study* study = seed.add_study();
    *study = baseStudy;
    study->set_name(
        base::StrCat({"max_version_study_", base::NumberToString(i)}));
    study->mutable_filter()->set_max_version(versions[i]);
  }

  // Add studies with invalid min_os_versions.
  for (size_t i = 0; i < std::size(versions); ++i) {
    Study* study = seed.add_study();
    *study = baseStudy;
    study->set_name(
        base::StrCat({"min_os_version_study_", base::NumberToString(i)}));
    study->mutable_filter()->set_min_os_version(versions[i]);
  }

  // Add studies with invalid max_os_versions.
  for (size_t i = 0; i < std::size(versions); ++i) {
    Study* study = seed.add_study();
    *study = baseStudy;
    study->set_name(
        base::StrCat({"max_os_version_study_", base::NumberToString(i)}));
    study->mutable_filter()->set_max_os_version(versions[i]);
  }

  auto client_state = CreateDummyClientFilterableState();
  client_state->locale = "en-CA";
  client_state->reference_date = base::Time::Now();
  client_state->version = base::Version("20.0.0.0");
  client_state->channel = Study::STABLE;
  client_state->form_factor = Study::DESKTOP;
  client_state->platform = Study::PLATFORM_ANDROID;
  client_state->os_version = base::Version("1.2.3");

  base::HistogramTester histogram_tester;
  std::vector<ProcessedStudy> processed_studies =
      FilterAndValidateStudies(seed, *client_state, VariationsLayers());

  ASSERT_EQ(0U, processed_studies.size());
  histogram_tester.ExpectTotalCount("Variations.InvalidStudyReason",
                                    std::size(versions) * 4);
  histogram_tester.ExpectBucketCount("Variations.InvalidStudyReason", 0,
                                     std::size(versions));
  histogram_tester.ExpectBucketCount("Variations.InvalidStudyReason", 1,
                                     std::size(versions));
  histogram_tester.ExpectBucketCount("Variations.InvalidStudyReason", 2,
                                     std::size(versions));
  histogram_tester.ExpectBucketCount("Variations.InvalidStudyReason", 3,
                                     std::size(versions));
}

TEST(VariationsStudyFilteringTest, FilterAndValidateStudiesWithBlankStudyName) {
  VariationsSeed seed;
  Study* study = seed.add_study();
  study->set_name("");
  study->set_default_experiment_name("Default");
  AddExperiment("A", 100, study);
  AddExperiment("Default", 0, study);

  study->mutable_filter()->add_platform(Study::PLATFORM_ANDROID);

  auto client_state = CreateDummyClientFilterableState();
  client_state->locale = "en-CA";
  client_state->reference_date = base::Time::Now();
  client_state->version = base::Version("20.0.0.0");
  client_state->channel = Study::STABLE;
  client_state->form_factor = Study::PHONE;
  client_state->platform = Study::PLATFORM_ANDROID;

  base::HistogramTester histogram_tester;
  std::vector<ProcessedStudy> processed_studies =
      FilterAndValidateStudies(seed, *client_state, VariationsLayers());

  ASSERT_EQ(0U, processed_studies.size());
  histogram_tester.ExpectUniqueSample("Variations.InvalidStudyReason", 8, 1);
}

TEST(VariationsStudyFilteringTest, FilterAndValidateStudiesWithCountry) {
  const char kSessionCountry[] = "ca";
  const char kPermanentCountry[] = "us";

  struct {
    Study::Consistency consistency;
    const char* filter_country;
    const char* filter_exclude_country;
    bool expect_study_kept;
  } test_cases[] = {
      // Country-agnostic studies should be kept regardless of country.
      {Study::SESSION, nullptr, nullptr, true},
      {Study::PERMANENT, nullptr, nullptr, true},

      // Session-consistency studies should obey the country code in the seed.
      {Study::SESSION, kSessionCountry, nullptr, true},
      {Study::SESSION, nullptr, kSessionCountry, false},
      {Study::SESSION, kPermanentCountry, nullptr, false},
      {Study::SESSION, nullptr, kPermanentCountry, true},

      // Permanent-consistency studies should obey the permanent-consistency
      // country code.
      {Study::PERMANENT, kPermanentCountry, nullptr, true},
      {Study::PERMANENT, nullptr, kPermanentCountry, false},
      {Study::PERMANENT, kSessionCountry, nullptr, false},
      {Study::PERMANENT, nullptr, kSessionCountry, true},
  };

  for (const auto& test : test_cases) {
    VariationsSeed seed;
    Study* study = seed.add_study();
    study->set_name("study");
    study->set_default_experiment_name("Default");
    AddExperiment("Default", 100, study);
    study->set_consistency(test.consistency);
    study->mutable_filter()->add_platform(Study::PLATFORM_ANDROID);
    if (test.filter_country)
      study->mutable_filter()->add_country(test.filter_country);
    if (test.filter_exclude_country)
      study->mutable_filter()->add_exclude_country(test.filter_exclude_country);

    auto client_state = CreateDummyClientFilterableState();
    client_state->locale = "en-CA";
    client_state->reference_date = base::Time::Now();
    client_state->version = base::Version("20.0.0.0");
    client_state->channel = Study::STABLE;
    client_state->form_factor = Study::PHONE;
    client_state->platform = Study::PLATFORM_ANDROID;
    client_state->session_consistency_country = kSessionCountry;
    client_state->permanent_consistency_country = kPermanentCountry;

    std::vector<ProcessedStudy> processed_studies =
        FilterAndValidateStudies(seed, *client_state, VariationsLayers());

    EXPECT_EQ(test.expect_study_kept, !processed_studies.empty());
  }
}

TEST(VariationsStudyFilteringTest, GetClientCountryForStudy_Session) {
  auto client_state = CreateDummyClientFilterableState();
  client_state->session_consistency_country = "session_country";
  client_state->permanent_consistency_country = "permanent_country";

  Study study;
  study.set_consistency(Study::SESSION);
  EXPECT_EQ("session_country",
            internal::GetClientCountryForStudy(study, *client_state));
}

TEST(VariationsStudyFilteringTest, GetClientCountryForStudy_Permanent) {
  auto client_state = CreateDummyClientFilterableState();
  client_state->session_consistency_country = "session_country";
  client_state->permanent_consistency_country = "permanent_country";

  Study study;
  study.set_consistency(Study::PERMANENT);
  EXPECT_EQ("permanent_country",
            internal::GetClientCountryForStudy(study, *client_state));
}

TEST(VariationsStudyFilteringTest, ValidateStudy) {
  Study study;
  study.set_name("study");
  study.set_default_experiment_name("def");
  AddExperiment("abc", 100, &study);
  Study::Experiment* default_group = AddExperiment("def", 200, &study);

  ProcessedStudy processed_study;
  EXPECT_TRUE(processed_study.Init(&study));
  EXPECT_EQ(300, processed_study.total_probability());

  // Min version checks.
  study.mutable_filter()->set_min_version("1.2.3.*");
  EXPECT_TRUE(processed_study.Init(&study));
  study.mutable_filter()->set_min_version("1.*.3");
  EXPECT_FALSE(processed_study.Init(&study));
  study.mutable_filter()->set_min_version("1.2.3");
  EXPECT_TRUE(processed_study.Init(&study));

  // Max version checks.
  study.mutable_filter()->set_max_version("2.3.4.*");
  EXPECT_TRUE(processed_study.Init(&study));
  study.mutable_filter()->set_max_version("*.3");
  EXPECT_FALSE(processed_study.Init(&study));
  study.mutable_filter()->set_max_version("2.3.4");
  EXPECT_TRUE(processed_study.Init(&study));

  // A blank default study is allowed.
  study.clear_default_experiment_name();
  EXPECT_TRUE(processed_study.Init(&study));

  study.set_default_experiment_name("xyz");
  EXPECT_FALSE(processed_study.Init(&study));

  study.set_default_experiment_name("def");
  default_group->clear_name();
  EXPECT_FALSE(processed_study.Init(&study));

  default_group->set_name("def");
  EXPECT_TRUE(processed_study.Init(&study));
  Study::Experiment* repeated_group = study.add_experiment();
  repeated_group->set_name("abc");
  repeated_group->set_probability_weight(1);
  EXPECT_FALSE(processed_study.Init(&study));
}

}  // namespace variations
