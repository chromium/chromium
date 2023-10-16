// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/miracle_parameter/common/public/miracle_parameter.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_amount_of_physical_memory_override.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace miracle_parameter {

namespace {

// Call FieldTrialList::FactoryGetFieldTrial().
scoped_refptr<base::FieldTrial> CreateFieldTrial(
    const std::string& trial_name,
    int total_probability,
    const std::string& default_group_name) {
  base::MockEntropyProvider entropy_provider(0.9);
  return base::FieldTrialList::FactoryGetFieldTrial(
      trial_name, total_probability, default_group_name, entropy_provider);
}

const std::string BoolToString(bool value) {
  return value ? "true" : "false";
}

class ScopedNullCommandLineOverride {
 public:
  ScopedNullCommandLineOverride()
      : process_args_(base::CommandLine::ForCurrentProcess()->GetArgs()) {
    base::CommandLine::Reset();
  }
  ScopedNullCommandLineOverride(const ScopedNullCommandLineOverride&) = delete;
  ScopedNullCommandLineOverride& operator=(
      const ScopedNullCommandLineOverride&) = delete;
  ~ScopedNullCommandLineOverride() {
    base::CommandLine::Init(0, nullptr);
    base::CommandLine::ForCurrentProcess()->InitFromArgv(process_args_);
  }

 private:
  const base::CommandLine::StringVector process_args_;
};

}  // namespace

class MiracleParameterTest : public ::testing::Test {
 public:
  MiracleParameterTest() = default;

  MiracleParameterTest(const MiracleParameterTest&) = delete;
  MiracleParameterTest& operator=(const MiracleParameterTest&) = delete;

  ~MiracleParameterTest() override {
    // Ensure that the maps are cleared between tests, since they are stored as
    // process singletons.
    base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
  }

  void CreateFeatureWithTrial(const base::Feature& feature,
                              base::FeatureList::OverrideState override_state,
                              base::FieldTrial* trial) {
    auto feature_list = std::make_unique<base::FeatureList>();
    feature_list->RegisterFieldTrialOverride(feature.name, override_state,
                                             trial);
    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(MiracleParameterTest, MiracleParameterForString) {
  const char kAForLessThan512MB[] = "a-value-for-less-than-512mb";
  const char kAFor512MBTo1GB[] = "a-value-for-512mb-to-1gb";
  const char kAFor1GBTo2GB[] = "a-value-for-1gb-to-2gb";
  const char kAFor2GBTo4GB[] = "a-value-for-2gb-to-4gb";
  const char kAFor4GBTo8GB[] = "a-value-for-4gb-to-8gb";
  const char kAFor8GBTo16GB[] = "a-value-for-8gb-to-16gb";
  const char kAFor16GBAndAbove[] = "a-value-for-16gb-and-above";
  const char kAParamValue[] = "a-param-value";
  const char kCParamValue[] = "c-param-value";
  const char kADefault[] = "default-for-a";
  const char kBDefault[] = "default-for-b";
  const char kCDefault[] = "default-for-c";

  // Set up the field trial params.
  const std::string kTrialName = "TrialName";
  std::map<std::string, std::string> params;
  params["aForLessThan512MB"] = kAForLessThan512MB;
  params["aFor512MBTo1GB"] = kAFor512MBTo1GB;
  params["aFor1GBTo2GB"] = kAFor1GBTo2GB;
  params["aFor2GBTo4GB"] = kAFor2GBTo4GB;
  params["aFor4GBTo8GB"] = kAFor4GBTo8GB;
  params["aFor8GBTo16GB"] = kAFor8GBTo16GB;
  params["aFor16GBAndAbove"] = kAFor16GBAndAbove;
  params["a"] = kAParamValue;
  params["c"] = kCParamValue;
  base::AssociateFieldTrialParams(kTrialName, "A", params);
  scoped_refptr<base::FieldTrial> trial(CreateFieldTrial(
      kTrialName, /*total_probability=*/100, /*default_group_name=*/"A"));
  static BASE_FEATURE(kFeature, "TestFeature",
                      base::FEATURE_ENABLED_BY_DEFAULT);
  CreateFeatureWithTrial(kFeature, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
                         trial.get());

  auto GetParamA = [&]() {
    return GetMiracleParameterAsString(kFeature, "a", kADefault);
  };
  auto GetParamB = [&]() {
    return GetMiracleParameterAsString(kFeature, "b", kBDefault);
  };
  auto GetParamC = [&]() {
    return GetMiracleParameterAsString(kFeature, "c", kCDefault);
  };

  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory512MB - 1);
    EXPECT_EQ(kAForLessThan512MB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory512MB);
    EXPECT_EQ(kAFor512MBTo1GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory1GB - 1);
    EXPECT_EQ(kAFor512MBTo1GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory1GB);
    EXPECT_EQ(kAFor1GBTo2GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory2GB - 1);
    EXPECT_EQ(kAFor1GBTo2GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory2GB);
    EXPECT_EQ(kAFor2GBTo4GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory4GB - 1);
    EXPECT_EQ(kAFor2GBTo4GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory4GB);
    EXPECT_EQ(kAFor4GBTo8GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory8GB - 1);
    EXPECT_EQ(kAFor4GBTo8GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory8GB);
    EXPECT_EQ(kAFor8GBTo16GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory16GB - 1);
    EXPECT_EQ(kAFor8GBTo16GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory16GB);
    EXPECT_EQ(kAFor16GBAndAbove, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    ScopedNullCommandLineOverride null_command_line_override;
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory16GB);
    EXPECT_EQ(kAParamValue, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
}

TEST_F(MiracleParameterTest, MiracleParameterForDouble) {
  const double kAForLessThan512MB = 0.1;
  const double kAFor512MBTo1GB = 0.2;
  const double kAFor1GBTo2GB = 0.3;
  const double kAFor2GBTo4GB = 0.4;
  const double kAFor4GBTo8GB = 0.5;
  const double kAFor8GBTo16GB = 0.6;
  const double kAFor16GBAndAbove = 0.7;
  const double kAParamValue = 0.8;
  const double kCParamValue = 0.9;
  const double kADefault = 1.0;
  const double kBDefault = 1.1;
  const double kCDefault = 1.2;

  // Set up the field trial params.
  const std::string kTrialName = "TrialName";
  std::map<std::string, std::string> params;
  params["aForLessThan512MB"] = base::ToString(kAForLessThan512MB);
  params["aFor512MBTo1GB"] = base::ToString(kAFor512MBTo1GB);
  params["aFor1GBTo2GB"] = base::ToString(kAFor1GBTo2GB);
  params["aFor2GBTo4GB"] = base::ToString(kAFor2GBTo4GB);
  params["aFor4GBTo8GB"] = base::ToString(kAFor4GBTo8GB);
  params["aFor8GBTo16GB"] = base::ToString(kAFor8GBTo16GB);
  params["aFor16GBAndAbove"] = base::ToString(kAFor16GBAndAbove);
  params["a"] = base::ToString(kAParamValue);
  params["c"] = base::ToString(kCParamValue);
  base::AssociateFieldTrialParams(kTrialName, "A", params);
  scoped_refptr<base::FieldTrial> trial(CreateFieldTrial(
      kTrialName, /*total_probability=*/100, /*default_group_name=*/"A"));
  static BASE_FEATURE(kFeature, "TestFeature",
                      base::FEATURE_ENABLED_BY_DEFAULT);
  CreateFeatureWithTrial(kFeature, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
                         trial.get());

  auto GetParamA = [&]() {
    return GetMiracleParameterAsDouble(kFeature, "a", kADefault);
  };
  auto GetParamB = [&]() {
    return GetMiracleParameterAsDouble(kFeature, "b", kBDefault);
  };
  auto GetParamC = [&]() {
    return GetMiracleParameterAsDouble(kFeature, "c", kCDefault);
  };

  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory512MB - 1);
    EXPECT_EQ(kAForLessThan512MB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory512MB);
    EXPECT_EQ(kAFor512MBTo1GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory1GB - 1);
    EXPECT_EQ(kAFor512MBTo1GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory1GB);
    EXPECT_EQ(kAFor1GBTo2GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory2GB - 1);
    EXPECT_EQ(kAFor1GBTo2GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory2GB);
    EXPECT_EQ(kAFor2GBTo4GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory4GB - 1);
    EXPECT_EQ(kAFor2GBTo4GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory4GB);
    EXPECT_EQ(kAFor4GBTo8GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory8GB - 1);
    EXPECT_EQ(kAFor4GBTo8GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory8GB);
    EXPECT_EQ(kAFor8GBTo16GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory16GB - 1);
    EXPECT_EQ(kAFor8GBTo16GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory16GB);
    EXPECT_EQ(kAFor16GBAndAbove, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    ScopedNullCommandLineOverride null_command_line_override;
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory16GB);
    EXPECT_EQ(kAParamValue, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
}

TEST_F(MiracleParameterTest, MiracleParameterForInt) {
  const int kAForLessThan512MB = 1;
  const int kAFor512MBTo1GB = 2;
  const int kAFor1GBTo2GB = 3;
  const int kAFor2GBTo4GB = 4;
  const int kAFor4GBTo8GB = 5;
  const int kAFor8GBTo16GB = 6;
  const int kAFor16GBAndAbove = 7;
  const int kAParamValue = 8;
  const int kCParamValue = 9;
  const int kADefault = 10;
  const int kBDefault = 11;
  const int kCDefault = 12;

  // Set up the field trial params.
  const std::string kTrialName = "TrialName";
  std::map<std::string, std::string> params;
  params["aForLessThan512MB"] = base::ToString(kAForLessThan512MB);
  params["aFor512MBTo1GB"] = base::ToString(kAFor512MBTo1GB);
  params["aFor1GBTo2GB"] = base::ToString(kAFor1GBTo2GB);
  params["aFor2GBTo4GB"] = base::ToString(kAFor2GBTo4GB);
  params["aFor4GBTo8GB"] = base::ToString(kAFor4GBTo8GB);
  params["aFor8GBTo16GB"] = base::ToString(kAFor8GBTo16GB);
  params["aFor16GBAndAbove"] = base::ToString(kAFor16GBAndAbove);
  params["a"] = base::ToString(kAParamValue);
  params["c"] = base::ToString(kCParamValue);
  base::AssociateFieldTrialParams(kTrialName, "A", params);
  scoped_refptr<base::FieldTrial> trial(CreateFieldTrial(
      kTrialName, /*total_probability=*/100, /*default_group_name=*/"A"));
  static BASE_FEATURE(kFeature, "TestFeature",
                      base::FEATURE_ENABLED_BY_DEFAULT);
  CreateFeatureWithTrial(kFeature, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
                         trial.get());

  auto GetParamA = [&]() {
    return GetMiracleParameterAsInt(kFeature, "a", kADefault);
  };
  auto GetParamB = [&]() {
    return GetMiracleParameterAsInt(kFeature, "b", kBDefault);
  };
  auto GetParamC = [&]() {
    return GetMiracleParameterAsInt(kFeature, "c", kCDefault);
  };

  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory512MB - 1);
    EXPECT_EQ(kAForLessThan512MB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory512MB);
    EXPECT_EQ(kAFor512MBTo1GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory1GB - 1);
    EXPECT_EQ(kAFor512MBTo1GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory1GB);
    EXPECT_EQ(kAFor1GBTo2GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory2GB - 1);
    EXPECT_EQ(kAFor1GBTo2GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory2GB);
    EXPECT_EQ(kAFor2GBTo4GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory4GB - 1);
    EXPECT_EQ(kAFor2GBTo4GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory4GB);
    EXPECT_EQ(kAFor4GBTo8GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory8GB - 1);
    EXPECT_EQ(kAFor4GBTo8GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory8GB);
    EXPECT_EQ(kAFor8GBTo16GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory16GB - 1);
    EXPECT_EQ(kAFor8GBTo16GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory16GB);
    EXPECT_EQ(kAFor16GBAndAbove, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    ScopedNullCommandLineOverride null_command_line_override;
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory16GB);
    EXPECT_EQ(kAParamValue, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
}

TEST_F(MiracleParameterTest, MiracleParameterForBool) {
  const bool kAForLessThan512MB = true;
  const bool kAFor512MBTo1GB = false;
  const bool kAFor1GBTo2GB = true;
  const bool kAFor2GBTo4GB = false;
  const bool kAFor4GBTo8GB = true;
  const bool kAFor8GBTo16GB = false;
  const bool kAFor16GBAndAbove = true;
  const bool kAParamValue = false;
  const bool kCParamValue = true;
  const bool kADefault = false;
  const bool kBDefault = true;
  const bool kCDefault = false;

  // Set up the field trial params.
  const std::string kTrialName = "TrialName";
  std::map<std::string, std::string> params;
  params["aForLessThan512MB"] = BoolToString(kAForLessThan512MB);
  params["aFor512MBTo1GB"] = BoolToString(kAFor512MBTo1GB);
  params["aFor1GBTo2GB"] = BoolToString(kAFor1GBTo2GB);
  params["aFor2GBTo4GB"] = BoolToString(kAFor2GBTo4GB);
  params["aFor4GBTo8GB"] = BoolToString(kAFor4GBTo8GB);
  params["aFor8GBTo16GB"] = BoolToString(kAFor8GBTo16GB);
  params["aFor16GBAndAbove"] = BoolToString(kAFor16GBAndAbove);
  params["a"] = BoolToString(kAParamValue);
  params["c"] = BoolToString(kCParamValue);
  base::AssociateFieldTrialParams(kTrialName, "A", params);
  scoped_refptr<base::FieldTrial> trial(CreateFieldTrial(
      kTrialName, /*total_probability=*/100, /*default_group_name=*/"A"));
  static BASE_FEATURE(kFeature, "TestFeature",
                      base::FEATURE_ENABLED_BY_DEFAULT);
  CreateFeatureWithTrial(kFeature, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
                         trial.get());

  auto GetParamA = [&]() {
    return GetMiracleParameterAsBool(kFeature, "a", kADefault);
  };
  auto GetParamB = [&]() {
    return GetMiracleParameterAsBool(kFeature, "b", kBDefault);
  };
  auto GetParamC = [&]() {
    return GetMiracleParameterAsBool(kFeature, "c", kCDefault);
  };

  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory512MB - 1);
    EXPECT_EQ(kAForLessThan512MB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory512MB);
    EXPECT_EQ(kAFor512MBTo1GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory1GB - 1);
    EXPECT_EQ(kAFor512MBTo1GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory1GB);
    EXPECT_EQ(kAFor1GBTo2GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory2GB - 1);
    EXPECT_EQ(kAFor1GBTo2GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory2GB);
    EXPECT_EQ(kAFor2GBTo4GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory4GB - 1);
    EXPECT_EQ(kAFor2GBTo4GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory4GB);
    EXPECT_EQ(kAFor4GBTo8GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory8GB - 1);
    EXPECT_EQ(kAFor4GBTo8GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory8GB);
    EXPECT_EQ(kAFor8GBTo16GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory16GB - 1);
    EXPECT_EQ(kAFor8GBTo16GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory16GB);
    EXPECT_EQ(kAFor16GBAndAbove, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    ScopedNullCommandLineOverride null_command_line_override;
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory16GB);
    EXPECT_EQ(kAParamValue, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
}

TEST_F(MiracleParameterTest, MiracleParameterForTimeDelta) {
  const base::TimeDelta kAForLessThan512MB = base::Seconds(1);
  const base::TimeDelta kAFor512MBTo1GB = base::Seconds(2);
  const base::TimeDelta kAFor1GBTo2GB = base::Seconds(3);
  const base::TimeDelta kAFor2GBTo4GB = base::Seconds(4);
  const base::TimeDelta kAFor4GBTo8GB = base::Seconds(5);
  const base::TimeDelta kAFor8GBTo16GB = base::Seconds(6);
  const base::TimeDelta kAFor16GBAndAbove = base::Seconds(7);
  const base::TimeDelta kAParamValue = base::Seconds(8);
  const base::TimeDelta kCParamValue = base::Seconds(9);
  const base::TimeDelta kADefault = base::Seconds(10);
  const base::TimeDelta kBDefault = base::Seconds(11);
  const base::TimeDelta kCDefault = base::Seconds(12);

  // Set up the field trial params.
  const std::string kTrialName = "TrialName";
  std::map<std::string, std::string> params;
  params["aForLessThan512MB"] = "1s";
  params["aFor512MBTo1GB"] = "2s";
  params["aFor1GBTo2GB"] = "3s";
  params["aFor2GBTo4GB"] = "4s";
  params["aFor4GBTo8GB"] = "5s";
  params["aFor8GBTo16GB"] = "6s";
  params["aFor16GBAndAbove"] = "7s";
  params["a"] = "8s";
  params["c"] = "9s";
  base::AssociateFieldTrialParams(kTrialName, "A", params);
  scoped_refptr<base::FieldTrial> trial(CreateFieldTrial(
      kTrialName, /*total_probability=*/100, /*default_group_name=*/"A"));
  static BASE_FEATURE(kFeature, "TestFeature",
                      base::FEATURE_ENABLED_BY_DEFAULT);
  CreateFeatureWithTrial(kFeature, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
                         trial.get());

  auto GetParamA = [&]() {
    return GetMiracleParameterAsTimeDelta(kFeature, "a", kADefault);
  };
  auto GetParamB = [&]() {
    return GetMiracleParameterAsTimeDelta(kFeature, "b", kBDefault);
  };
  auto GetParamC = [&]() {
    return GetMiracleParameterAsTimeDelta(kFeature, "c", kCDefault);
  };

  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory512MB - 1);
    EXPECT_EQ(kAForLessThan512MB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory512MB);
    EXPECT_EQ(kAFor512MBTo1GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory1GB - 1);
    EXPECT_EQ(kAFor512MBTo1GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory1GB);
    EXPECT_EQ(kAFor1GBTo2GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory2GB - 1);
    EXPECT_EQ(kAFor1GBTo2GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory2GB);
    EXPECT_EQ(kAFor2GBTo4GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory4GB - 1);
    EXPECT_EQ(kAFor2GBTo4GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory4GB);
    EXPECT_EQ(kAFor4GBTo8GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory8GB - 1);
    EXPECT_EQ(kAFor4GBTo8GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory8GB);
    EXPECT_EQ(kAFor8GBTo16GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory16GB - 1);
    EXPECT_EQ(kAFor8GBTo16GB, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory16GB);
    EXPECT_EQ(kAFor16GBAndAbove, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
  {
    ScopedNullCommandLineOverride null_command_line_override;
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory16GB);
    EXPECT_EQ(kAParamValue, GetParamA());
    EXPECT_EQ(kBDefault, GetParamB());
    EXPECT_EQ(kCParamValue, GetParamC());
  }
}

TEST_F(MiracleParameterTest, MiracleParameterForEnum) {
  enum class ParamEnum {
    kAForLessThan512MB,
    kAFor512MBTo1GB,
    kAFor1GBTo2GB,
    kAFor2GBTo4GB,
    kAFor4GBTo8GB,
    kAFor8GBTo16GB,
    kAFor16GBAndAbove,
    kAParamValue,
    kCParamValue,
    kADefault,
    kBDefault,
    kCDefault,
  };

  static const base::FeatureParam<ParamEnum>::Option param_enums[] = {
      {ParamEnum::kAForLessThan512MB, "a-value-for-less-than-512mb"},
      {ParamEnum::kAFor512MBTo1GB, "a-value-for-512mb-to-1gb"},
      {ParamEnum::kAFor1GBTo2GB, "a-value-for-1gb-to-2gb"},
      {ParamEnum::kAFor2GBTo4GB, "a-value-for-2gb-to-4gb"},
      {ParamEnum::kAFor4GBTo8GB, "a-value-for-4gb-to-8gb"},
      {ParamEnum::kAFor8GBTo16GB, "a-value-for-8gb-to-16gb"},
      {ParamEnum::kAFor16GBAndAbove, "a-value-for-16gb-and-above"},
      {ParamEnum::kAParamValue, "a-param-value"},
      {ParamEnum::kCParamValue, "c-param-value"},
      {ParamEnum::kADefault, "default-for-a"},
      {ParamEnum::kBDefault, "default-for-b"},
      {ParamEnum::kCDefault, "default-for-c"},
  };

  // Set up the field trial params.
  const std::string kTrialName = "TrialName";
  std::map<std::string, std::string> params;
  params["aForLessThan512MB"] = "a-value-for-less-than-512mb";
  params["aFor512MBTo1GB"] = "a-value-for-512mb-to-1gb";
  params["aFor1GBTo2GB"] = "a-value-for-1gb-to-2gb";
  params["aFor2GBTo4GB"] = "a-value-for-2gb-to-4gb";
  params["aFor4GBTo8GB"] = "a-value-for-4gb-to-8gb";
  params["aFor8GBTo16GB"] = "a-value-for-8gb-to-16gb";
  params["aFor16GBAndAbove"] = "a-value-for-16gb-and-above";
  params["a"] = "a-param-value";
  params["c"] = "c-param-value";
  base::AssociateFieldTrialParams(kTrialName, "A", params);
  scoped_refptr<base::FieldTrial> trial(CreateFieldTrial(
      kTrialName, /*total_probability=*/100, /*default_group_name=*/"A"));
  static BASE_FEATURE(kFeature, "TestFeature",
                      base::FEATURE_ENABLED_BY_DEFAULT);
  CreateFeatureWithTrial(kFeature, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
                         trial.get());

  auto GetParamA = [&]() {
    return GetMiracleParameterAsEnum(kFeature, "a", ParamEnum::kADefault,
                                     base::make_span(param_enums));
  };
  auto GetParamB = [&]() {
    return GetMiracleParameterAsEnum(kFeature, "b", ParamEnum::kBDefault,
                                     base::make_span(param_enums));
  };
  auto GetParamC = [&]() {
    return GetMiracleParameterAsEnum(kFeature, "c", ParamEnum::kCDefault,
                                     base::make_span(param_enums));
  };

  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory512MB - 1);
    EXPECT_EQ(ParamEnum::kAForLessThan512MB, GetParamA());
    EXPECT_EQ(ParamEnum::kBDefault, GetParamB());
    EXPECT_EQ(ParamEnum::kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory512MB);
    EXPECT_EQ(ParamEnum::kAFor512MBTo1GB, GetParamA());
    EXPECT_EQ(ParamEnum::kBDefault, GetParamB());
    EXPECT_EQ(ParamEnum::kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory1GB - 1);
    EXPECT_EQ(ParamEnum::kAFor512MBTo1GB, GetParamA());
    EXPECT_EQ(ParamEnum::kBDefault, GetParamB());
    EXPECT_EQ(ParamEnum::kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory1GB);
    EXPECT_EQ(ParamEnum::kAFor1GBTo2GB, GetParamA());
    EXPECT_EQ(ParamEnum::kBDefault, GetParamB());
    EXPECT_EQ(ParamEnum::kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory2GB - 1);
    EXPECT_EQ(ParamEnum::kAFor1GBTo2GB, GetParamA());
    EXPECT_EQ(ParamEnum::kBDefault, GetParamB());
    EXPECT_EQ(ParamEnum::kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory2GB);
    EXPECT_EQ(ParamEnum::kAFor2GBTo4GB, GetParamA());
    EXPECT_EQ(ParamEnum::kBDefault, GetParamB());
    EXPECT_EQ(ParamEnum::kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory4GB - 1);
    EXPECT_EQ(ParamEnum::kAFor2GBTo4GB, GetParamA());
    EXPECT_EQ(ParamEnum::kBDefault, GetParamB());
    EXPECT_EQ(ParamEnum::kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory4GB);
    EXPECT_EQ(ParamEnum::kAFor4GBTo8GB, GetParamA());
    EXPECT_EQ(ParamEnum::kBDefault, GetParamB());
    EXPECT_EQ(ParamEnum::kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory8GB - 1);
    EXPECT_EQ(ParamEnum::kAFor4GBTo8GB, GetParamA());
    EXPECT_EQ(ParamEnum::kBDefault, GetParamB());
    EXPECT_EQ(ParamEnum::kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory8GB);
    EXPECT_EQ(ParamEnum::kAFor8GBTo16GB, GetParamA());
    EXPECT_EQ(ParamEnum::kBDefault, GetParamB());
    EXPECT_EQ(ParamEnum::kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory16GB - 1);
    EXPECT_EQ(ParamEnum::kAFor8GBTo16GB, GetParamA());
    EXPECT_EQ(ParamEnum::kBDefault, GetParamB());
    EXPECT_EQ(ParamEnum::kCParamValue, GetParamC());
  }
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory16GB);
    EXPECT_EQ(ParamEnum::kAFor16GBAndAbove, GetParamA());
    EXPECT_EQ(ParamEnum::kBDefault, GetParamB());
    EXPECT_EQ(ParamEnum::kCParamValue, GetParamC());
  }
  {
    ScopedNullCommandLineOverride null_command_line_override;
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
        kMiracleParameterMemory16GB);
    EXPECT_EQ(ParamEnum::kAParamValue, GetParamA());
    EXPECT_EQ(ParamEnum::kBDefault, GetParamB());
    EXPECT_EQ(ParamEnum::kCParamValue, GetParamC());
  }
}

}  // namespace miracle_parameter
