// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/study_filtering.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/processed_study.h"
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

}  // namespace

TEST(VariationsStudyFilteringTest, CheckStudyChannel) {
  const Study::Channel channels[] = {
      Study::CANARY, Study::DEV, Study::BETA, Study::STABLE,
  };
  bool channel_added[base::size(channels)] = {0};

  Study::Filter filter;

  // Check in the forwarded order. The loop cond is <= base::size(channels)
  // instead of < so that the result of adding the last channel gets checked.
  for (size_t i = 0; i <= base::size(channels); ++i) {
    for (size_t j = 0; j < base::size(channels); ++j) {
      const bool expected = channel_added[j] || filter.channel_size() == 0;
      const bool result = internal::CheckStudyChannel(filter, channels[j]);
      EXPECT_EQ(expected, result) << "Case " << i << "," << j << " failed!";
    }

    if (i < base::size(channels)) {
      filter.add_channel(channels[i]);
      channel_added[i] = true;
    }
  }

  // Do the same check in the reverse order.
  filter.clear_channel();
  memset(&channel_added, 0, sizeof(channel_added));
  for (size_t i = 0; i <= base::size(channels); ++i) {
    for (size_t j = 0; j < base::size(channels); ++j) {
      const bool expected = channel_added[j] || filter.channel_size() == 0;
      const bool result = internal::CheckStudyChannel(filter, channels[j]);
      EXPECT_EQ(expected, result) << "Case " << i << "," << j << " failed!";
    }

    if (i < base::size(channels)) {
      const int index = base::size(channels) - i - 1;
      filter.add_channel(channels[index]);
      channel_added[index] = true;
    }
  }
}

TEST(VariationsStudyFilteringTest, CheckStudyFormFactor) {
  const Study::FormFactor form_factors[] = {
      Study::DESKTOP, Study::PHONE, Study::TABLET, Study::KIOSK,
  };

  ASSERT_EQ(Study::FormFactor_ARRAYSIZE,
            static_cast<int>(base::size(form_factors)));

  bool form_factor_added[base::size(form_factors)] = {0};
  Study::Filter filter;

  for (size_t i = 0; i <= base::size(form_factors); ++i) {
    for (size_t j = 0; j < base::size(form_factors); ++j) {
      const bool expected = form_factor_added[j] ||
                            filter.form_factor_size() == 0;
      const bool result = internal::CheckStudyFormFactor(filter,
                                                         form_factors[j]);
      EXPECT_EQ(expected, result) << "form_factor: case " << i << "," << j
                                  << " failed!";
    }

    if (i < base::size(form_factors)) {
      filter.add_form_factor(form_factors[i]);
      form_factor_added[i] = true;
    }
  }

  // Do the same check in the reverse order.
  filter.clear_form_factor();
  memset(&form_factor_added, 0, sizeof(form_factor_added));
  for (size_t i = 0; i <= base::size(form_factors); ++i) {
    for (size_t j = 0; j < base::size(form_factors); ++j) {
      const bool expected = form_factor_added[j] ||
                            filter.form_factor_size() == 0;
      const bool result = internal::CheckStudyFormFactor(filter,
                                                         form_factors[j]);
      EXPECT_EQ(expected, result) << "form_factor: case " << i << "," << j
                                  << " failed!";
    }

    if (i < base::size(form_factors)) {
      const int index = base::size(form_factors) - i - 1;
      filter.add_form_factor(form_factors[index]);
      form_factor_added[index] = true;
    }
  }

  // Test exclude_form_factors, forward order.
  filter.clear_form_factor();
  bool form_factor_excluded[base::size(form_factors)] = {0};
  for (size_t i = 0; i <= base::size(form_factors); ++i) {
    for (size_t j = 0; j < base::size(form_factors); ++j) {
      const bool expected = filter.exclude_form_factor_size() == 0 ||
                            !form_factor_excluded[j];
      const bool result = internal::CheckStudyFormFactor(filter,
                                                         form_factors[j]);
      EXPECT_EQ(expected, result) << "exclude_form_factor: case " << i << ","
                                  << j << " failed!";
    }

    if (i < base::size(form_factors)) {
      filter.add_exclude_form_factor(form_factors[i]);
      form_factor_excluded[i] = true;
    }
  }

  // Test exclude_form_factors, reverse order.
  filter.clear_exclude_form_factor();
  memset(&form_factor_excluded, 0, sizeof(form_factor_excluded));
  for (size_t i = 0; i <= base::size(form_factors); ++i) {
    for (size_t j = 0; j < base::size(form_factors); ++j) {
      const bool expected = filter.exclude_form_factor_size() == 0 ||
                            !form_factor_excluded[j];
      const bool result = internal::CheckStudyFormFactor(filter,
                                                         form_factors[j]);
      EXPECT_EQ(expected, result) << "exclude_form_factor: case " << i << ","
                                  << j << " failed!";
    }

    if (i < base::size(form_factors)) {
      const int index = base::size(form_factors) - i - 1;
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
  const Study::Platform platforms[] = {
      Study::PLATFORM_WINDOWS,         Study::PLATFORM_MAC,
      Study::PLATFORM_LINUX,           Study::PLATFORM_CHROMEOS,
      Study::PLATFORM_ANDROID,         Study::PLATFORM_IOS,
      Study::PLATFORM_ANDROID_WEBVIEW, Study::PLATFORM_FUCHSIA};
  ASSERT_EQ(Study::Platform_ARRAYSIZE, static_cast<int>(base::size(platforms)));
  bool platform_added[base::size(platforms)] = {0};

  Study::Filter filter;

  // Check in the forwarded order. The loop cond is <= base::size(platforms)
  // instead of < so that the result of adding the last platform gets checked.
  for (size_t i = 0; i <= base::size(platforms); ++i) {
    for (size_t j = 0; j < base::size(platforms); ++j) {
      const bool expected = platform_added[j];
      const bool result = internal::CheckStudyPlatform(filter, platforms[j]);
      EXPECT_EQ(expected, result) << "Case " << i << "," << j << " failed!";
    }

    if (i < base::size(platforms)) {
      filter.add_platform(platforms[i]);
      platform_added[i] = true;
    }
  }

  // Do the same check in the reverse order.
  filter.clear_platform();
  memset(&platform_added, 0, sizeof(platform_added));
  for (size_t i = 0; i <= base::size(platforms); ++i) {
    for (size_t j = 0; j < base::size(platforms); ++j) {
      const bool expected = platform_added[j];
      const bool result = internal::CheckStudyPlatform(filter, platforms[j]);
      EXPECT_EQ(expected, result) << "Case " << i << "," << j << " failed!";
    }

    if (i < base::size(platforms)) {
      const int index = base::size(platforms) - i - 1;
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
      base::BindOnce([] { return false; }));
  ClientFilterableState client_enterprise(base::BindOnce([] { return true; }));

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

TEST(VariationsStudyFilteringTest, CheckStudyStartDate) {
  const base::Time now = base::Time::Now();
  const base::TimeDelta delta = base::TimeDelta::FromHours(1);
  const struct {
    const base::Time start_date;
    bool expected_result;
  } start_test_cases[] = {
      {now - delta, true},
      // Note, the proto start_date is truncated to seconds, but the reference
      // date isn't.
      {now, true},
      {now + delta, false},
  };

  Study::Filter filter;

  // Start date not set should result in true.
  EXPECT_TRUE(internal::CheckStudyStartDate(filter, now));

  for (size_t i = 0; i < base::size(start_test_cases); ++i) {
    filter.set_start_date(TimeToProtoTime(start_test_cases[i].start_date));
    const bool result = internal::CheckStudyStartDate(filter, now);
    EXPECT_EQ(start_test_cases[i].expected_result, result)
        << "Case " << i << " failed!";
  }
}

TEST(VariationsStudyFilteringTest, CheckStudyEndDate) {
  const base::Time now = base::Time::Now();
  const base::TimeDelta delta = base::TimeDelta::FromHours(1);
  const struct {
    const base::Time end_date;
    bool expected_result;
  } start_test_cases[] = {
      {now - delta, false}, {now + delta, true},
  };

  Study::Filter filter;

  // End date not set should result in true.
  EXPECT_TRUE(internal::CheckStudyEndDate(filter, now));

  for (size_t i = 0; i < base::size(start_test_cases); ++i) {
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
  } min_test_cases[] = {
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

  const struct {
    const char* max_os_version;
    const char* os_version;
    bool expected_result;
  } max_test_cases[] = {
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

  Study::Filter filter;

  // Min/max version not set should result in true.
  EXPECT_TRUE(internal::CheckStudyOSVersion(filter, base::Version("1.2.3")));

  for (size_t i = 0; i < base::size(min_test_cases); ++i) {
    filter.set_min_os_version(min_test_cases[i].min_os_version);
    const bool result = internal::CheckStudyOSVersion(
        filter, base::Version(min_test_cases[i].os_version));
    EXPECT_EQ(min_test_cases[i].expected_result, result)
        << "Min OS version case " << i << " failed!";
  }
  filter.clear_min_os_version();

  for (size_t i = 0; i < base::size(max_test_cases); ++i) {
    filter.set_max_os_version(max_test_cases[i].max_os_version);
    const bool result = internal::CheckStudyOSVersion(
        filter, base::Version(max_test_cases[i].os_version));
    EXPECT_EQ(max_test_cases[i].expected_result, result)
        << "Max OS version case " << i << " failed!";
  }

  // Check intersection semantics.
  for (size_t i = 0; i < base::size(min_test_cases); ++i) {
    for (size_t j = 0; j < base::size(max_test_cases); ++j) {
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
  } min_test_cases[] = {
    { "1.2.2", "1.2.3", true },
    { "1.2.3", "1.2.3", true },
    { "1.2.4", "1.2.3", false },
    { "1.3.2", "1.2.3", false },
    { "2.1.2", "1.2.3", false },
    { "0.3.4", "1.2.3", true },
    // Wildcards.
    { "1.*", "1.2.3", true },
    { "1.2.*", "1.2.3", true },
    { "1.2.3.*", "1.2.3", true },
    { "1.2.4.*", "1.2.3", false },
    { "2.*", "1.2.3", false },
    { "0.3.*", "1.2.3", true },
  };

  const struct {
    const char* max_version;
    const char* version;
    bool expected_result;
  } max_test_cases[] = {
    { "1.2.2", "1.2.3", false },
    { "1.2.3", "1.2.3", true },
    { "1.2.4", "1.2.3", true },
    { "2.1.1", "1.2.3", true },
    { "2.1.1", "2.3.4", false },
    // Wildcards
    { "2.1.*", "2.3.4", false },
    { "2.*", "2.3.4", true },
    { "2.3.*", "2.3.4", true },
    { "2.3.4.*", "2.3.4", true },
    { "2.3.4.0.*", "2.3.4", true },
    { "2.4.*", "2.3.4", true },
    { "1.3.*", "2.3.4", false },
    { "1.*", "2.3.4", false },
  };

  Study::Filter filter;

  // Min/max version not set should result in true.
  EXPECT_TRUE(internal::CheckStudyVersion(filter, base::Version("1.2.3")));

  for (size_t i = 0; i < base::size(min_test_cases); ++i) {
    filter.set_min_version(min_test_cases[i].min_version);
    const bool result = internal::CheckStudyVersion(
        filter, base::Version(min_test_cases[i].version));
    EXPECT_EQ(min_test_cases[i].expected_result, result) <<
        "Min. version case " << i << " failed!";
  }
  filter.clear_min_version();

  for (size_t i = 0; i < base::size(max_test_cases); ++i) {
    filter.set_max_version(max_test_cases[i].max_version);
    const bool result = internal::CheckStudyVersion(
        filter, base::Version(max_test_cases[i].version));
    EXPECT_EQ(max_test_cases[i].expected_result, result) <<
        "Max version case " << i << " failed!";
  }

  // Check intersection semantics.
  for (size_t i = 0; i < base::size(min_test_cases); ++i) {
    for (size_t j = 0; j < base::size(max_test_cases); ++j) {
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
    for (const auto& hw_class : SplitFilterString(test.exclude_hardware_class))
      filter.add_exclude_hardware_class(hw_class);

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

  ClientFilterableState client_state({});
  client_state.locale = "en-CA";
  client_state.reference_date = base::Time::Now();
  client_state.version = base::Version("20.0.0.0");
  client_state.channel = Study::STABLE;
  client_state.form_factor = Study::DESKTOP;
  client_state.platform = Study::PLATFORM_ANDROID;

  std::vector<ProcessedStudy> processed_studies;
  FilterAndValidateStudies(seed, client_state, &processed_studies);

  // Check that only the first kTrial1Name study was kept.
  ASSERT_EQ(2U, processed_studies.size());
  EXPECT_EQ(kTrial1Name, processed_studies[0].study()->name());
  EXPECT_EQ(kGroup1Name, processed_studies[0].study()->experiment(0).name());
  EXPECT_EQ(kTrial3Name, processed_studies[1].study()->name());
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

    ClientFilterableState client_state({});
    client_state.locale = "en-CA";
    client_state.reference_date = base::Time::Now();
    client_state.version = base::Version("20.0.0.0");
    client_state.channel = Study::STABLE;
    client_state.form_factor = Study::PHONE;
    client_state.platform = Study::PLATFORM_ANDROID;
    client_state.session_consistency_country = kSessionCountry;
    client_state.permanent_consistency_country = kPermanentCountry;

    std::vector<ProcessedStudy> processed_studies;
    FilterAndValidateStudies(seed, client_state, &processed_studies);

    EXPECT_EQ(test.expect_study_kept, !processed_studies.empty());
  }
}

TEST(VariationsStudyFilteringTest, GetClientCountryForStudy_Session) {
  ClientFilterableState client_state({});
  client_state.session_consistency_country = "session_country";
  client_state.permanent_consistency_country = "permanent_country";

  Study study;
  study.set_consistency(Study::SESSION);
  EXPECT_EQ("session_country",
            internal::GetClientCountryForStudy(study, client_state));
}

TEST(VariationsStudyFilteringTest, GetClientCountryForStudy_Permanent) {
  ClientFilterableState client_state({});
  client_state.session_consistency_country = "session_country";
  client_state.permanent_consistency_country = "permanent_country";

  Study study;
  study.set_consistency(Study::PERMANENT);
  EXPECT_EQ("permanent_country",
            internal::GetClientCountryForStudy(study, client_state));
}

TEST(VariationsStudyFilteringTest, IsStudyExpired) {
  const base::Time now = base::Time::Now();
  const base::TimeDelta delta = base::TimeDelta::FromHours(1);
  const struct {
    const base::Time expiry_date;
    bool expected_result;
  } expiry_test_cases[] = {
    { now - delta, true },
    { now, true },
    { now + delta, false },
  };

  Study study;

  // Expiry date not set should result in false.
  EXPECT_FALSE(internal::IsStudyExpired(study, now));

  for (size_t i = 0; i < base::size(expiry_test_cases); ++i) {
    study.set_expiry_date(TimeToProtoTime(expiry_test_cases[i].expiry_date));
    const bool result = internal::IsStudyExpired(study, now);
    EXPECT_EQ(expiry_test_cases[i].expected_result, result)
        << "Case " << i << " failed!";
  }
}

TEST(VariationsStudyFilteringTest, ValidateStudy) {
  Study study;
  study.set_default_experiment_name("def");
  AddExperiment("abc", 100, &study);
  Study::Experiment* default_group = AddExperiment("def", 200, &study);

  ProcessedStudy processed_study;
  EXPECT_TRUE(processed_study.Init(&study, false));
  EXPECT_EQ(300, processed_study.total_probability());

  // Min version checks.
  study.mutable_filter()->set_min_version("1.2.3.*");
  EXPECT_TRUE(processed_study.Init(&study, false));
  study.mutable_filter()->set_min_version("1.*.3");
  EXPECT_FALSE(processed_study.Init(&study, false));
  study.mutable_filter()->set_min_version("1.2.3");
  EXPECT_TRUE(processed_study.Init(&study, false));

  // Max version checks.
  study.mutable_filter()->set_max_version("2.3.4.*");
  EXPECT_TRUE(processed_study.Init(&study, false));
  study.mutable_filter()->set_max_version("*.3");
  EXPECT_FALSE(processed_study.Init(&study, false));
  study.mutable_filter()->set_max_version("2.3.4");
  EXPECT_TRUE(processed_study.Init(&study, false));

  // A blank default study is allowed.
  study.clear_default_experiment_name();
  EXPECT_TRUE(processed_study.Init(&study, false));

  study.set_default_experiment_name("xyz");
  EXPECT_FALSE(processed_study.Init(&study, false));

  study.set_default_experiment_name("def");
  default_group->clear_name();
  EXPECT_FALSE(processed_study.Init(&study, false));

  default_group->set_name("def");
  EXPECT_TRUE(processed_study.Init(&study, false));
  Study::Experiment* repeated_group = study.add_experiment();
  repeated_group->set_name("abc");
  repeated_group->set_probability_weight(1);
  EXPECT_FALSE(processed_study.Init(&study, false));
}

}  // namespace variations
