// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/cros_evaluate_seed/evaluate_seed.h"

#include <optional>

#include "base/base64.h"
#include "base/base_switches.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_file.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/branding_buildflags.h"
#include "build/config/chromebox_for_meetings/buildflags.h"
#include "chromeos/ash/components/dbus/featured/featured.pb.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/cros_evaluate_seed/cros_variations_field_trial_creator.h"
#include "components/variations/field_trial_config/field_trial_util.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/variations_ids_provider.h"
#include "components/variations/variations_safe_seed_store_local_state.h"
#include "components/variations/variations_switches.h"
#include "components/variations/variations_test_utils.h"
#include "components/version_info/version_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

namespace variations::cros_early_boot::evaluate_seed {

namespace {
// Representation of:
//
// serial_number:
// "SMCi1oYXNoLzNhZGYxZmE3NjU1NDVhMmE3YmFhYWZiMmE4MzY4YWY4NjljN2UzNzgSAiAA#NlincGzr4JI="
// version: "hash/3adf1fa765545a2a7baaafb2a8368af869c7e378"
// Study data:
// name: "EarlyBootStudy"
// consistency: PERMANENT
// experiment: {
//   name: "Enabled"
//   probability_weight: 100
//   feature_association: {
//     enable_feature: "CrOSEarlyBootTestFeature"
//   }
//   param: {
//     name: "baz"
//     value: "quux"
//   }
// }

const char kEarlyBootTestSeed_Uncompressed[] =
    "CldTTUNpMW9ZWE5vTHpOaFpHWXhabUUzTmpVMU5EVmhNbUUzWW1GaFlXWmlNbUU0TXpZNFlXWT"
    "ROamxqTjJVek56Z1NCQmg1SUFBPSNObGluY0d6cjRKST0SSAoORWFybHlCb290U3R1ZHk4AUo0"
    "CgdFbmFibGVkEGQyCwoDYmF6EgRxdXV4YhoKGENyT1NFYXJseUJvb3RUZXN0RmVhdHVyZSItaG"
    "FzaC8zYWRmMWZhNzY1NTQ1YTJhN2JhYWFmYjJhODM2OGFmODY5YzdlMzc4";

const char kEarlyBootTestSeed_Compressed[] =
    "H4sIAAAAAAAAAD3LQW+CMBgA0CwuMemSxXAyHrfzskALdAcP4qpipB4Qsdy+SlkhIBlCgv31u+"
    "3dH0rjaF3arbjw9mC4zrZizBqGeZXY/Puso4Zh0Wy0SLMyahiJjCAiFYRXdcWdxHDzEweBdsPV"
    "avnO6/J23ZqO7MOltUOvDLr6EbRtH/dD/qBPe4Km7AayVvksd17QRIKxnn+HYZQLNF93x/h/nN"
    "S93yjoh069fWi4608MeWEX4HuuS1xwwJcAUEgHKPYoFNT7uvoK+/QP+y9VX9IAAAA=";

const char kEarlyBootTestSeed_Signature[] =
    "MEYCIQDKAjErS8+3NkSOv9tGTeoqGxc3sjie/secLtlfI8qj7wIhAJ02TwZ07Ijdl6V/izOdDk"
    "n8Ro5D1nVUI6raiapKze/n";

const char* kEarlyBootTestSeed_StudyNames[] = {"EarlyBootStudy"};

const SignedSeedData kEarlyBootTestData{
    kEarlyBootTestSeed_StudyNames, kEarlyBootTestSeed_Uncompressed,
    kEarlyBootTestSeed_Compressed, kEarlyBootTestSeed_Signature};

// Create mock testing config equivalent to:
// {
//   "CrOSEarlyBootTestStudy": [
//       {
//           "platforms": [
//               "android",
//               "android_weblayer",
//               "android_webview",
//               "chromeos",
//               "chromeos_lacros",
//               "fuchsia",
//               "ios",
//               "linux",
//               "mac",
//               "windows"
//           ],
//           "experiments": [
//               {
//                   "name": "Enabled",
//                   "params": {
//                       "fieldtrial_test_key": "fieldtrial_test_value"
//                   },
//                   "enable_features": [
//                       "CrOSEarlyBootTestFeature"
//                   ]
//               }
//           ]
//       }
//   ]
// }

const Study::Platform array_kEarlyBootFieldTrialConfig_platforms[] = {
    Study::PLATFORM_ANDROID,
    Study::PLATFORM_ANDROID_WEBLAYER,
    Study::PLATFORM_ANDROID_WEBVIEW,
    Study::PLATFORM_CHROMEOS,
    Study::PLATFORM_CHROMEOS_LACROS,
    Study::PLATFORM_FUCHSIA,
    Study::PLATFORM_IOS,
    Study::PLATFORM_LINUX,
    Study::PLATFORM_MAC,
    Study::PLATFORM_WINDOWS,
};

const char* early_boot_enable_features[] = {"CrOSEarlyBootTestFeature"};
const FieldTrialTestingExperimentParams
    array_kEarlyBootFieldTrialConfig_params[] = {
        {
            "fieldtrial_test_key",
            "fieldtrial_test_value",
        },
};

const FieldTrialTestingExperiment
    array_kEarlyBootFieldTrialConfig_experiments[] = {
        {/*name=*/"Enabled",
         /*platforms=*/array_kEarlyBootFieldTrialConfig_platforms,
         /*form_factors=*/{},
         /*is_low_end_device=*/std::nullopt,
         /*min_os_version=*/nullptr,
         /*params=*/array_kEarlyBootFieldTrialConfig_params,
         /*enable_features=*/early_boot_enable_features,
         /*disable_features=*/{},
         /*forcing_flag=*/{},
         /*override_ui_string=*/{}}};

const FieldTrialTestingStudy array_kEarlyBootFieldTrialConfig_studies[] = {
    {/*name=*/"CrOSEarlyBootTestStudy",
     /*experiments=*/array_kEarlyBootFieldTrialConfig_experiments},
};

const FieldTrialTestingConfig kEarlyBootTestingConfig = {
    array_kEarlyBootFieldTrialConfig_studies};

std::unique_ptr<ClientFilterableState> GetBasicClientFilterableState() {
  CrosVariationsServiceClient client;
  TestingPrefServiceSimple prefs;
  metrics::MetricsService::RegisterPrefs(prefs.registry());
  ::variations::VariationsService::RegisterPrefs(prefs.registry());
  std::unique_ptr<CrOSVariationsFieldTrialCreator> creator =
      GetFieldTrialCreator(&prefs, &client, /*safe_seed_details=*/std::nullopt);

  return creator->GetClientFilterableStateForVersion(
      version_info::GetVersion());
}

// Largely copied from
// content/shell/browser/shell_content_browser_client.cc's CreateLocalState.
std::unique_ptr<PrefService> CreateStateWriter(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const base::FilePath& local_state_path) {
  auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();

  metrics::MetricsService::RegisterPrefs(pref_registry.get());
  ::variations::VariationsService::RegisterPrefs(pref_registry.get());

  PrefServiceFactory pref_service_factory;
  auto local_state_pref_store = base::MakeRefCounted<JsonPrefStore>(
      local_state_path, /*pref_filter=*/nullptr, task_runner);
  auto error = local_state_pref_store->ReadPrefs();
  if (error != JsonPrefStore::PREF_READ_ERROR_NONE) {
    LOG(ERROR) << "failed to read prefs " << error;
    return nullptr;
  }

  pref_service_factory.set_user_prefs(local_state_pref_store);

  return pref_service_factory.Create(pref_registry);
}

std::optional<std::string> DecodeBase64AndDecompress(
    const std::string& b64_compressed) {
  std::string decoded;
  if (!base::Base64Decode(b64_compressed, &decoded)) {
    return std::nullopt;
  }
  std::string result;
  if (!compression::GzipUncompress(decoded, &result)) {
    return std::nullopt;
  }
  return result;
}

class TestCrOSVariationsFieldTrialCreator
    : public CrOSVariationsFieldTrialCreator {
 public:
  TestCrOSVariationsFieldTrialCreator(
      VariationsServiceClient* client,
      std::unique_ptr<VariationsSeedStore> seed_store)
      : CrOSVariationsFieldTrialCreator(client, std::move(seed_store)) {}

  TestCrOSVariationsFieldTrialCreator(
      const TestCrOSVariationsFieldTrialCreator&) = delete;
  TestCrOSVariationsFieldTrialCreator& operator=(
      const TestCrOSVariationsFieldTrialCreator&) = delete;

  ~TestCrOSVariationsFieldTrialCreator() override = default;

 protected:
  // We override this method so that a mock testing config is used instead of
  // the one defined in fieldtrial_testing_config.json.
  void ApplyFieldTrialTestingConfig(base::FeatureList* feature_list) override {
    AssociateParamsFromFieldTrialConfig(kEarlyBootTestingConfig,
                                        base::DoNothing(), GetPlatform(),
                                        GetCurrentFormFactor(), feature_list);
  }
};

// Create a TestCrOSVariationsFieldTrialCreator. Exposed to use as a callback.
std::unique_ptr<CrOSVariationsFieldTrialCreator>
CreateTestCrOSVariationsFieldTrialCreator(
    PrefService* local_state,
    CrosVariationsServiceClient* client,
    const std::optional<featured::SeedDetails>& safe_seed_details) {
  // This argument is not needed. It is only included for compatibility with the
  // non-test signature.
  (void)safe_seed_details;

  auto safe_seed =
      std::make_unique<VariationsSafeSeedStoreLocalState>(local_state);
  auto seed_store =
      std::make_unique<VariationsSeedStore>(local_state, std::move(safe_seed));
  return std::make_unique<TestCrOSVariationsFieldTrialCreator>(
      client, std::move(seed_store));
}

}  // namespace

using ::base::test::EqualsProto;

TEST(VariationsCrosEvaluateSeed, GetClientFilterable_Enrolled) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv({"evaluate_seed"});
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      kEnterpriseEnrolledSwitch);
  CrosVariationsServiceClient client;
  EXPECT_TRUE(client.IsEnterprise());
}

TEST(VariationsCrosEvaluateSeed, GetClientFilterable_NotEnrolled) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv({"evaluate_seed"});
  CrosVariationsServiceClient client;
  EXPECT_FALSE(client.IsEnterprise());
}

struct Param {
  std::string test_name;
  std::string channel_name;
  Study::Channel channel;
};

class VariationsCrosEvaluateSeedGetChannel
    : public ::testing::TestWithParam<Param> {
 protected:
  VariationsCrosEvaluateSeedGetChannel() = default;
};

TEST_P(VariationsCrosEvaluateSeedGetChannel,
       GetClientFilterableState_Channel_Override) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv({"evaluate_seed"});
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kFakeVariationsChannel, GetParam().channel_name);

  std::unique_ptr<ClientFilterableState> state =
      GetBasicClientFilterableState();
  EXPECT_EQ(GetParam().channel, state->channel);
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Verify GetClientFilterableState gets the channel from lsb-release on branded
// builds.
TEST_P(VariationsCrosEvaluateSeedGetChannel,
       GetClientFilterableState_Channel_Branded) {
  std::string lsb_release = base::StrCat(
      {"CHROMEOS_RELEASE_TRACK=", GetParam().channel_name, "-channel"});
  const base::Time lsb_release_time(
      base::Time::FromSecondsSinceUnixEpoch(12345.6));
  base::test::ScopedChromeOSVersionInfo lsb_info(lsb_release, lsb_release_time);

  base::CommandLine::ForCurrentProcess()->InitFromArgv({"evaluate_seed"});

  std::unique_ptr<ClientFilterableState> state =
      GetBasicClientFilterableState();
  EXPECT_EQ(GetParam().channel, state->channel);
}

#else   // BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Verify that we use unknown channel on non-branded builds.
TEST(VariationsCrosEvaluateSeed, GetClientFilterableState_Channel_NotBranded) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv({"evaluate_seed"});

  std::unique_ptr<ClientFilterableState> state =
      GetBasicClientFilterableState();
  EXPECT_EQ(Study::UNKNOWN, state->channel);
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
INSTANTIATE_TEST_SUITE_P(
    VariationsCrosEvaluateSeedGetChannel,
    VariationsCrosEvaluateSeedGetChannel,
    ::testing::ValuesIn<Param>({{"Stable", "stable", Study::STABLE},
                                {"Beta", "beta", Study::BETA},
                                {"Dev", "dev", Study::DEV},
                                {"Canary", "canary", Study::CANARY},
                                {"Unknown", "testimage", Study::UNKNOWN}}),
    [](const ::testing::TestParamInfo<
        VariationsCrosEvaluateSeedGetChannel::ParamType>& info) {
      return info.param.test_name;
    });

#if BUILDFLAG(PLATFORM_CFM)
TEST(VariationsCrosEvaluateSeed, GetClientFilterableState_FormFactor) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv({"evaluate_seed"});
  CrosVariationsServiceClient client;
  EXPECT_EQ(Study::MEET_DEVICE, client.GetCurrentFormFactor());
}
#else   // BUILDFLAG(PLATFORM_CFM)
TEST(VariationsCrosEvaluateSeed, GetClientFilterableState_FormFactor) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv({"evaluate_seed"});
  CrosVariationsServiceClient client;
  EXPECT_EQ(Study::DESKTOP, client.GetCurrentFormFactor());
}
#endif  // BUILDFLAG(PLATFORM_CFM)

// Should ignore data if flag is off.
TEST(VariationsCrosEvaluateSeed, GetSafeSeedData_Off) {
  featured::SeedDetails safe_seed;
  safe_seed.set_b64_compressed_data("some text");
  std::string text;
  safe_seed.SerializeToString(&text);
  FILE* stream = fmemopen(text.data(), text.size(), "r");
  ASSERT_NE(stream, nullptr);

  base::CommandLine::ForCurrentProcess()->InitFromArgv({"evaluate_seed"});
  auto data = GetSafeSeedData(stream);
  featured::SeedDetails empty_seed;
  ASSERT_TRUE(data.has_value());
  EXPECT_FALSE(data.value().use_safe_seed);
  EXPECT_THAT(data.value().seed_data, EqualsProto(empty_seed));
}

// Should return specified data via stream if flag is on.
TEST(VariationsCrosEvaluateSeed, GetSafeSeedData_On) {
  featured::SeedDetails safe_seed;
  safe_seed.set_b64_compressed_data("some text");
  std::string text;
  safe_seed.SerializeToString(&text);
  FILE* stream = fmemopen(text.data(), text.size(), "r");
  ASSERT_NE(stream, nullptr);

  base::CommandLine::ForCurrentProcess()->InitFromArgv({"evaluate_seed"});
  base::CommandLine::ForCurrentProcess()->AppendSwitch(kSafeSeedSwitch);
  auto data = GetSafeSeedData(stream);
  ASSERT_TRUE(data.has_value());
  EXPECT_TRUE(data.value().use_safe_seed);
  EXPECT_THAT(data.value().seed_data, EqualsProto(safe_seed));
}

// Should not attempt to read stream if flag is not on.
TEST(VariationsCrosEvaluateSeed, GetSafeSeedData_Off_FailRead) {
  featured::SeedDetails safe_seed;
  safe_seed.set_b64_compressed_data("some text");
  std::string text;
  safe_seed.SerializeToString(&text);
  FILE* stream = fmemopen(text.data(), text.size(), "w");
  ASSERT_NE(stream, nullptr);

  base::CommandLine::ForCurrentProcess()->InitFromArgv({"evaluate_seed"});
  auto data = GetSafeSeedData(stream);
  featured::SeedDetails empty_seed;
  ASSERT_TRUE(data.has_value());
  EXPECT_FALSE(data.value().use_safe_seed);
  EXPECT_THAT(data.value().seed_data, EqualsProto(empty_seed));
}

// If flag is on and reading fails, should return nullopt.
TEST(VariationsCrosEvaluateSeed, GetSafeSeedData_On_FailRead) {
  featured::SeedDetails safe_seed;
  safe_seed.set_b64_compressed_data("some text");
  std::string text;
  safe_seed.SerializeToString(&text);
  FILE* stream = fmemopen(text.data(), text.size(), "w");
  ASSERT_NE(stream, nullptr);

  base::CommandLine::ForCurrentProcess()->InitFromArgv({"evaluate_seed"});
  base::CommandLine::ForCurrentProcess()->AppendSwitch(kSafeSeedSwitch);
  auto data = GetSafeSeedData(stream);
  EXPECT_FALSE(data.has_value());
}

// If flag is on and parsing input fails, should return nullopt.
TEST(VariationsCrosEvaluateSeed, GetSafeSeedData_On_FailParse) {
  std::string text("not a serialized proto");
  FILE* stream = fmemopen(text.data(), text.size(), "r");
  ASSERT_NE(stream, nullptr);

  base::CommandLine::ForCurrentProcess()->InitFromArgv({"evaluate_seed"});
  base::CommandLine::ForCurrentProcess()->AppendSwitch(kSafeSeedSwitch);
  auto data = GetSafeSeedData(stream);
  ASSERT_FALSE(data.has_value());
}

// If flag is on and reading fails, should return nullopt.
TEST(VariationsCrosEvaluateSeed, GetSafeSeedData_On_FailRead_Null) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv({"evaluate_seed"});
  base::CommandLine::ForCurrentProcess()->AppendSwitch(kSafeSeedSwitch);
  auto data = GetSafeSeedData(nullptr);
  EXPECT_FALSE(data.has_value());
}

class VariationsCrosEvaluateSeedMainTest : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(local_state_.Create());
    base::FilePath path = local_state_.path();
    ASSERT_TRUE(base::WriteFile(path, "{}"));

    local_state_writer_ =
        CreateStateWriter(task_environment_.GetMainThreadTaskRunner(), path);
    // By default, write a common seed that doesn't have our special experiments
    // (e.g. CrOSEarlyBootTestFeature) in it.
    WriteSeedData(local_state_writer_.get(), ::variations::kTestSeedData,
                  kRegularSeedPrefKeys);
    // Ensure that the write persists and the executor finishes executing it.
    task_environment_.RunUntilIdle();

    ASSERT_TRUE(out_file_.Create());

    // These tests validate the setup features and field trials: initialize
    // them to null on each test to mimic fresh startup.
    scoped_feature_list_.InitWithNullFeatureAndFieldTrialLists();

    // Set up command command-line switches for all subsequent tests.
    base::CommandLine::ForCurrentProcess()->InitFromArgv({"evaluate_seed"});
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableFieldTrialTestingConfig);
    base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
        kLocalStatePathSwitch, local_state_.path());
  }

  void TearDown() override {
    // Tear down VariationsIdsProvider so it doesn't CHECK-fail on subsequent
    // tests.
    variations::VariationsIdsProvider::DestroyInstanceForTesting();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempFile local_state_;
  base::ScopedTempFile out_file_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<PrefService> local_state_writer_;
};

TEST_F(VariationsCrosEvaluateSeedMainTest, Main_NoSafeSeedFlag) {
  FILE* out_stream = fopen(out_file_.path().value().c_str(), "w");
  ASSERT_NE(out_stream, nullptr);
  EXPECT_EQ(EXIT_SUCCESS, EvaluateSeedMain(nullptr, out_stream));
}

// Verify that EvaluateSeedMain respects kEnableFeatures.
TEST_F(VariationsCrosEvaluateSeedMainTest, Main_NoSafeSeedFlag_EnableFeatures) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ::switches::kEnableFeatures, "CrOSEarlyBootTestFeature:foo/bar");

  FILE* out_stream = fopen(out_file_.path().value().c_str(), "w");
  ASSERT_NE(out_stream, nullptr);
  EXPECT_EQ(EXIT_SUCCESS, EvaluateSeedMain(nullptr, out_stream));

  // Check that the feature was correctly serialized.
  std::string serialized_proto;
  ASSERT_TRUE(base::ReadFileToString(out_file_.path(), &serialized_proto));

  featured::ComputedState read_output;
  ASSERT_TRUE(read_output.ParseFromString(serialized_proto));
  ASSERT_EQ(read_output.overrides_size(), 1);
  const featured::FeatureOverride& feature = read_output.overrides(0);
  EXPECT_EQ(feature.name(), "CrOSEarlyBootTestFeature");
  EXPECT_EQ(feature.override_state(), featured::OVERRIDE_ENABLE_FEATURE);
  // These names are auto-generated from the feature name in
  // base/feature_list.cc in ParseEnableFeatures().
  EXPECT_EQ(feature.trial_name(), "StudyCrOSEarlyBootTestFeature");
  EXPECT_EQ(feature.group_name(), "GroupCrOSEarlyBootTestFeature");
  ASSERT_EQ(feature.params_size(), 1);
  EXPECT_EQ(feature.params(0).key(), "foo");
  EXPECT_EQ(feature.params(0).value(), "bar");
}

// Verify that EvaluateSeedMain respects kForceFieldTrials and
// kForceFieldTrialParams.
TEST_F(VariationsCrosEvaluateSeedMainTest,
       Main_NoSafeSeedFlag_ForceFieldTrials) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ::switches::kForceFieldTrials, "ATrial/AGroup/");
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kForceFieldTrialParams, "ATrial.AGroup:foo/bar");
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ::switches::kEnableFeatures, "CrOSEarlyBootTestFeature<ATrial");

  FILE* out_stream = fopen(out_file_.path().value().c_str(), "w");
  ASSERT_NE(out_stream, nullptr);
  EXPECT_EQ(EXIT_SUCCESS, EvaluateSeedMain(nullptr, out_stream));

  // Check that the feature was correctly serialized.
  std::string serialized_proto;
  ASSERT_TRUE(base::ReadFileToString(out_file_.path(), &serialized_proto));

  featured::ComputedState read_output;
  ASSERT_TRUE(read_output.ParseFromString(serialized_proto));
  ASSERT_EQ(read_output.overrides_size(), 1);
  const featured::FeatureOverride& feature = read_output.overrides(0);
  EXPECT_EQ(feature.name(), "CrOSEarlyBootTestFeature");
  EXPECT_EQ(feature.override_state(), featured::OVERRIDE_ENABLE_FEATURE);
  EXPECT_EQ(feature.trial_name(), "ATrial");
  EXPECT_EQ(feature.group_name(), "AGroup");
  ASSERT_EQ(feature.params_size(), 1);
  EXPECT_EQ(feature.params(0).key(), "foo");
  EXPECT_EQ(feature.params(0).value(), "bar");
}

// Test that evaluating a seed normally works (i.e. no safe mode, no override
// flags, just reading the seed from local state).
TEST_F(VariationsCrosEvaluateSeedMainTest, Main_NoSafeSeedFlag_NormalSeed) {
  WriteSeedData(local_state_writer_.get(), kEarlyBootTestData,
                kRegularSeedPrefKeys);
  task_environment_.RunUntilIdle();

  FILE* out_stream = fopen(out_file_.path().value().c_str(), "w");
  ASSERT_NE(out_stream, nullptr);
  EXPECT_EQ(EXIT_SUCCESS, EvaluateSeedMain(nullptr, out_stream));

  // Check that the feature was correctly serialized.
  std::string serialized_proto;
  ASSERT_TRUE(base::ReadFileToString(out_file_.path(), &serialized_proto));

  featured::ComputedState read_output;
  ASSERT_TRUE(read_output.ParseFromString(serialized_proto));
  ASSERT_EQ(read_output.overrides_size(), 1);
  const featured::FeatureOverride& feature = read_output.overrides(0);
  EXPECT_EQ(feature.name(), "CrOSEarlyBootTestFeature");
  EXPECT_EQ(feature.override_state(), featured::OVERRIDE_ENABLE_FEATURE);
  EXPECT_EQ(feature.trial_name(), "EarlyBootStudy");
  EXPECT_EQ(feature.group_name(), "Enabled");
  ASSERT_EQ(feature.params_size(), 1);
  EXPECT_EQ(feature.params(0).key(), "baz");
  EXPECT_EQ(feature.params(0).value(), "quux");

  // gzip does not promise a stable serialization, so de-b64 and decompress
  // before comparing.
  auto decompressed_actual =
      DecodeBase64AndDecompress(read_output.used_seed().b64_compressed_data());
  ASSERT_TRUE(decompressed_actual.has_value());
  auto decompressed_expected =
      DecodeBase64AndDecompress(kEarlyBootTestSeed_Compressed);
  ASSERT_TRUE(decompressed_expected.has_value());
  EXPECT_EQ(decompressed_actual.value(), decompressed_expected.value());

  EXPECT_EQ(read_output.used_seed().signature(), kEarlyBootTestSeed_Signature);
}

// Test that evaluating a safe seed works (with no filters).
TEST_F(VariationsCrosEvaluateSeedMainTest, Main_SafeSeed_Evaluate) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(kSafeSeedSwitch);

  featured::SeedDetails safe_seed;
  safe_seed.set_b64_compressed_data(kEarlyBootTestSeed_Compressed);
  safe_seed.set_signature(kEarlyBootTestSeed_Signature);
  std::string text;
  safe_seed.SerializeToString(&text);
  FILE* in_stream = fmemopen(text.data(), text.size(), "r");
  ASSERT_NE(in_stream, nullptr);

  FILE* out_stream = fopen(out_file_.path().value().c_str(), "w");
  ASSERT_NE(out_stream, nullptr);
  EXPECT_EQ(EXIT_SUCCESS, EvaluateSeedMain(in_stream, out_stream));

  // Check that the feature was correctly serialized.
  std::string serialized_proto;
  ASSERT_TRUE(base::ReadFileToString(out_file_.path(), &serialized_proto));

  featured::ComputedState read_output;
  ASSERT_TRUE(read_output.ParseFromString(serialized_proto));
  ASSERT_EQ(read_output.overrides_size(), 1);
  const featured::FeatureOverride& feature = read_output.overrides(0);
  EXPECT_EQ(feature.name(), "CrOSEarlyBootTestFeature");
  EXPECT_EQ(feature.override_state(), featured::OVERRIDE_ENABLE_FEATURE);
  EXPECT_EQ(feature.trial_name(), "EarlyBootStudy");
  EXPECT_EQ(feature.group_name(), "Enabled");
  ASSERT_EQ(feature.params_size(), 1);
  EXPECT_EQ(feature.params(0).key(), "baz");
  EXPECT_EQ(feature.params(0).value(), "quux");

  // gzip does not promise a stable serialization, so de-b64 and decompress
  // before comparing.
  auto decompressed_actual =
      DecodeBase64AndDecompress(read_output.used_seed().b64_compressed_data());
  ASSERT_TRUE(decompressed_actual.has_value());
  auto decompressed_expected =
      DecodeBase64AndDecompress(kEarlyBootTestSeed_Compressed);
  ASSERT_TRUE(decompressed_expected.has_value());
  EXPECT_EQ(decompressed_actual.value(), decompressed_expected.value());

  EXPECT_EQ(read_output.used_seed().signature(), kEarlyBootTestSeed_Signature);
}

// Verify that the FieldTrialTestingConfig is applied, rather than any seeds.
TEST_F(VariationsCrosEvaluateSeedMainTest, Main_FieldTrialConfig) {
  // This should be ignored.
  WriteSeedData(local_state_writer_.get(), kEarlyBootTestData,
                kRegularSeedPrefKeys);
  task_environment_.RunUntilIdle();

  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kDisableFieldTrialTestingConfig);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableFieldTrialTestingConfig);

  // This is required so that we can use our hard-coded fake
  // fieldtrial_testing_config (see kEarlyBootTestingConfig), rather than the
  // actual fieldtrial_testing_config.json.
  base::OnceCallback get_field_trial_creator =
      base::BindOnce(&CreateTestCrOSVariationsFieldTrialCreator);

  FILE* out_stream = fopen(out_file_.path().value().c_str(), "w");
  ASSERT_NE(out_stream, nullptr);
  EXPECT_EQ(EXIT_SUCCESS, EvaluateSeedMain(nullptr, out_stream,
                                           std::move(get_field_trial_creator)));

  // Check that the feature was correctly serialized.
  std::string serialized_proto;
  ASSERT_TRUE(base::ReadFileToString(out_file_.path(), &serialized_proto));

  featured::ComputedState read_output;
  ASSERT_TRUE(read_output.ParseFromString(serialized_proto));
  ASSERT_EQ(read_output.overrides_size(), 1);
  const featured::FeatureOverride& feature = read_output.overrides(0);
  EXPECT_EQ(feature.name(), "CrOSEarlyBootTestFeature");
  EXPECT_EQ(feature.override_state(), featured::OVERRIDE_ENABLE_FEATURE);
  EXPECT_EQ(feature.trial_name(), "CrOSEarlyBootTestStudy");
  EXPECT_EQ(feature.group_name(), "Enabled");
  ASSERT_EQ(feature.params_size(), 1);
  EXPECT_EQ(feature.params(0).key(), "fieldtrial_test_key");
  EXPECT_EQ(feature.params(0).value(), "fieldtrial_test_value");

  EXPECT_FALSE(read_output.has_used_seed());
}

TEST_F(VariationsCrosEvaluateSeedMainTest, Main_BadJson) {
  ASSERT_TRUE(base::WriteFile(local_state_.path(), "{"));

  FILE* out_stream = fopen(out_file_.path().value().c_str(), "w");
  ASSERT_NE(out_stream, nullptr);
  EXPECT_EQ(EXIT_FAILURE, EvaluateSeedMain(nullptr, out_stream));
}

TEST_F(VariationsCrosEvaluateSeedMainTest, Main_JsonNotDict) {
  ASSERT_TRUE(base::WriteFile(local_state_.path(), "[]"));

  FILE* out_stream = fopen(out_file_.path().value().c_str(), "w");
  ASSERT_NE(out_stream, nullptr);
  EXPECT_EQ(EXIT_FAILURE, EvaluateSeedMain(nullptr, out_stream));
}

TEST_F(VariationsCrosEvaluateSeedMainTest, Main_EmptyLocalState) {
  ASSERT_TRUE(base::WriteFile(local_state_.path(), "{}"));

  FILE* out_stream = fopen(out_file_.path().value().c_str(), "w");
  ASSERT_NE(out_stream, nullptr);
  EXPECT_EQ(EXIT_SUCCESS, EvaluateSeedMain(nullptr, out_stream));
}

TEST_F(VariationsCrosEvaluateSeedMainTest, Main_NoStdin) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(kSafeSeedSwitch);
  FILE* out_stream = fopen(out_file_.path().value().c_str(), "w");
  ASSERT_NE(out_stream, nullptr);
  EXPECT_EQ(EXIT_FAILURE, EvaluateSeedMain(nullptr, nullptr));
}

}  // namespace variations::cros_early_boot::evaluate_seed
