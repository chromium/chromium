// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_test_utils.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/time/time.h"
#include "components/metrics/clean_exit_beacon.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/field_trial_config/fieldtrial_testing_config.h"
#include "components/variations/hashing.h"
#include "components/variations/pref_names.h"
#include "components/variations/proto/client_variations.pb.h"
#include "components/variations/synthetic_trial_registry.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_switches.h"
#include "third_party/zlib/google/compression_utils.h"

namespace variations {
namespace {

// kTestSeed is a simple VariationsSeed containing:
// serial_number: "test"
// study: {
//   name: "UMA-Uniformity-Trial-50-Percent"
//   consistency: PERMANENT
//   experiment: {
//     name: "default"
//     probability_weight: 1
//   }
//   experiment: {
//     name: "group_01"
//     probability_weight: 1
//   }
// }

const char* kTestSeed_StudyNames[] = {"UMA-Uniformity-Trial-50-Percent"};

const char kTestSeed_Base64UncompressedData[] =
    "CgR0ZXN0Ej4KH1VNQS1Vbmlmb3JtaXR5LVRyaWFsLTUwLVBlcmNlbnQ4AUoLCgdkZWZhdWx0EA"
    "FKDAoIZ3JvdXBfMDEQAQ==";

const char kTestSeed_Base64CompressedData[] =
    "H4sIAAAAAAAA/+JiKUktLhGy45IP9XXUDc3LTMsvys0sqdQNKcpMzNE1NdANSC1KTs0rsWD04u"
    "ZiT0lNSyzNKRFg9OLh4kgvyi8tiDcwFGAEBAAA//90/JgERgAAAA==";

const char kTestSeed_Base64Signature[] =
    "MEUCIQD5AEAzk5qEuE3xOZl+xSZR15Ac1RJpsXMiou7i5W0sMAIgRn++ngh03HaMGC+Pjl9NOu"
    "Doxf83qsSwycF2PSS1nYQ=";

const char* kCrashingSeed_StudyNames[] = {"CrashingStudy"};

// kCrashingSeed is a VariationsSeed that triggers a crash for testing:
// serial_number:  "35ed2d9e354b414befdf930a734094019c0162f1"
// study:  {
//   name:  "CrashingStudy"
//   consistency:  PERMANENT
//   experiment:  {
//     name:  "EnabledLaunch"
//     probability_weight:  100
//     feature_association:  {
//       enable_feature:  "ForceFieldTrialSetupCrashForTesting"
//     }
//   }
//   experiment:  {
//     name:  "ForcedOn_ForceFieldTrialSetupCrashForTesting"
//     probability_weight:  0
//     feature_association:  {
//       forcing_feature_on:  "ForceFieldTrialSetupCrashForTesting"
//     }
//   }
//   experiment:  {
//     name:  "ForcedOff_ForceFieldTrialSetupCrashForTesting"
//     probability_weight:  0
//     feature_association:  {
//       forcing_feature_off:  "ForceFieldTrialSetupCrashForTesting"
//     }
//   }
//   filter:  {
//     min_version:  "91.*"
//     channel:  CANARY
//     channel:  DEV
//     channel:  BETA
//     channel:  STABLE
//     platform:  PLATFORM_ANDROID
//     platform:  PLATFORM_IOS
//     platform:  PLATFORM_ANDROID_WEBVIEW
//     platform:  PLATFORM_WINDOWS
//     platform:  PLATFORM_MAC
//     platform:  PLATFORM_LINUX
//     platform:  PLATFORM_CHROMEOS
//     platform:  PLATFORM_CHROMEOS_LACROS
//   }
// }
// version:  "hash/4aa56a1dc30dfc767615248d6fee29830198b276"

const char kCrashingSeed_Base64UncompressedData[] =
    "CigzNWVkMmQ5ZTM1NGI0MTRiZWZkZjkzMGE3MzQwOTQwMTljMDE2MmYxEp4CCg1DcmFzaGluZ1"
    "N0dWR5OAFKOAoNRW5hYmxlZExhdW5jaBBkYiUKI0ZvcmNlRmllbGRUcmlhbFNldHVwQ3Jhc2hG"
    "b3JUZXN0aW5nSlcKLEZvcmNlZE9uX0ZvcmNlRmllbGRUcmlhbFNldHVwQ3Jhc2hGb3JUZXN0aW"
    "5nEABiJRojRm9yY2VGaWVsZFRyaWFsU2V0dXBDcmFzaEZvclRlc3RpbmdKWAotRm9yY2VkT2Zm"
    "X0ZvcmNlRmllbGRUcmlhbFNldHVwQ3Jhc2hGb3JUZXN0aW5nEABiJSIjRm9yY2VGaWVsZFRyaW"
    "FsU2V0dXBDcmFzaEZvclRlc3RpbmdSHhIEOTEuKiAAIAEgAiADKAQoBSgGKAAoASgCKAMoCSIt"
    "aGFzaC80YWE1NmExZGMzMGRmYzc2NzYxNTI0OGQ2ZmVlMjk4MzAxOThiMjc2";

const char kCrashingSeed_Base64CompressedData[] =
    "H4sIAAAAAAAAAI3QwUvDMBTH8babwgKDsaMHKZNBEKdJk6bJWbbDEAQ30JskeS+2UKp07cF/"
    "Zn+rZfgH9Py+73P4ESpyhAwMilw6yaXDAMEIZgshmZGMG8+4ygJfnhMyf27tqayar0PXw6+"
    "O95rMt411NcKL7RtfLsCtyd3uu/W4q7CGY1vZ+oBd/"
    "3P5HA5HPHUDsH8nD5cMXpvPEf0icuubUfAH2fzDIYyVV2Pkt9vl1PDH+zRK4zRJJ3RKr+"
    "g1jWhMEzqhs9WmHPonaW2uLAcvGARfqELxPJMaVEDMjBbDotplhfoDs9NLbnoBAAA=";

const char kCrashingSeed_Base64Signature[] =
    "MEQCIEn1+VsBfNA93dxzpk+BLhdO91kMQnofxfTK5Uo8vDi8AiAnTCFCIPgEGWNOKzuKfNWn6"
    "emB6pnGWjSTbI/pvfxHnw==";

// Create mock testing config equivalent to:
// {
//   "UnitTest": [
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
//                       "x": "1"
//                   },
//                   "enable_features": [
//                       "UnitTestEnabled"
//                   ]
//               }
//           ]
//       }
//   ]
// }

const Study::Platform array_kFieldTrialConfig_platforms_0[] = {
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

const char* enable_features_0[] = {"UnitTestEnabled"};
const FieldTrialTestingExperimentParams array_kFieldTrialConfig_params_0[] = {
    {
        "x",
        "1",
    },
};

const FieldTrialTestingExperiment array_kFieldTrialConfig_experiments_0[] = {
    {/*name=*/"Enabled",
     /*platforms=*/array_kFieldTrialConfig_platforms_0,
     /*form_factors=*/{},
     /*is_low_end_device=*/std::nullopt,
     /*min_os_version=*/nullptr,
     /*params=*/array_kFieldTrialConfig_params_0,
     /*enable_features=*/enable_features_0,
     /*disable_features=*/{},
     /*forcing_flag=*/nullptr,
     /*override_ui_string=*/{}},
};

const FieldTrialTestingStudy array_kFieldTrialConfig_studies[] = {
    {/*name=*/"UnitTest",
     /*experiments=*/array_kFieldTrialConfig_experiments_0},
};

}  // namespace

const SignedSeedData kTestSeedData{
    kTestSeed_StudyNames, kTestSeed_Base64UncompressedData,
    kTestSeed_Base64CompressedData, kTestSeed_Base64Signature};

const SignedSeedData kCrashingSeedData{
    kCrashingSeed_StudyNames, kCrashingSeed_Base64UncompressedData,
    kCrashingSeed_Base64CompressedData, kCrashingSeed_Base64Signature};

const SignedSeedPrefKeys kSafeSeedPrefKeys{prefs::kVariationsSafeCompressedSeed,
                                           prefs::kVariationsSafeSeedSignature};

const SignedSeedPrefKeys kRegularSeedPrefKeys{prefs::kVariationsCompressedSeed,
                                              prefs::kVariationsSeedSignature};

SignedSeedData::SignedSeedData(base::span<const char*> in_study_names,
                               const char* in_base64_uncompressed_data,
                               const char* in_base64_compressed_data,
                               const char* in_base64_signature)
    : study_names(std::move(in_study_names)),
      base64_uncompressed_data(in_base64_uncompressed_data),
      base64_compressed_data(in_base64_compressed_data),
      base64_signature(in_base64_signature) {}

SignedSeedData::~SignedSeedData() = default;

SignedSeedData::SignedSeedData(const SignedSeedData&) = default;
SignedSeedData::SignedSeedData(SignedSeedData&&) = default;
SignedSeedData& SignedSeedData::operator=(const SignedSeedData&) = default;
SignedSeedData& SignedSeedData::operator=(SignedSeedData&&) = default;

void DisableTestingConfig() {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableFieldTrialTestingConfig);
}

void EnableTestingConfig() {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableFieldTrialTestingConfig);
}

bool ExtractVariationIds(const std::string& variations,
                         std::set<VariationID>* variation_ids,
                         std::set<VariationID>* trigger_ids) {
  std::string serialized_proto;
  if (!base::Base64Decode(variations, &serialized_proto))
    return false;
  ClientVariations proto;
  if (!proto.ParseFromString(serialized_proto))
    return false;
  for (int i = 0; i < proto.variation_id_size(); ++i)
    variation_ids->insert(proto.variation_id(i));
  for (int i = 0; i < proto.trigger_variation_id_size(); ++i)
    trigger_ids->insert(proto.trigger_variation_id(i));
  return true;
}

scoped_refptr<base::FieldTrial> CreateTrialAndAssociateId(
    const std::string& trial_name,
    const std::string& default_group_name,
    IDCollectionKey key,
    VariationID id) {
  AssociateGoogleVariationID(key, trial_name, default_group_name, id);
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::CreateFieldTrial(trial_name, default_group_name));
  DCHECK(trial);

  if (trial) {
    // Ensure the trial is registered under the correct key so we can look it
    // up.
    trial->Activate();
  }

  return trial;
}

void SimulateCrash(PrefService* local_state) {
  local_state->SetBoolean(metrics::prefs::kStabilityExitedCleanly, false);
  metrics::CleanExitBeacon::SkipCleanShutdownStepsForTesting();
}

void WriteSeedData(PrefService* local_state,
                   const SignedSeedData& seed_data,
                   const SignedSeedPrefKeys& pref_keys) {
  local_state->SetString(pref_keys.base64_compressed_data_key,
                         seed_data.base64_compressed_data);
  local_state->SetString(pref_keys.base64_signature_key,
                         seed_data.base64_signature);
  local_state->CommitPendingWrite();
}

bool FieldTrialListHasAllStudiesFrom(const SignedSeedData& seed_data) {
  return base::ranges::all_of(seed_data.study_names, [](const char* study) {
    return base::FieldTrialList::TrialExists(study);
  });
}

void ResetVariations() {
  testing::ClearAllVariationIDs();
  testing::ClearAllVariationParams();
}

const FieldTrialTestingConfig kTestingConfig = {
    array_kFieldTrialConfig_studies};

std::unique_ptr<ClientFilterableState> CreateDummyClientFilterableState() {
  auto client_state = std::make_unique<ClientFilterableState>(
      base::BindOnce([] { return false; }),
      base::BindOnce([] { return base::flat_set<uint64_t>(); }));
  client_state->locale = "en-CA";
  client_state->reference_date = base::Time::Now();
  client_state->version = base::Version("20.0.0.0");
  client_state->channel = Study::STABLE;
  client_state->form_factor = Study::PHONE;
  client_state->platform = Study::PLATFORM_ANDROID;
  return client_state;
}

// Constructs mocked EntropyProviders.
MockEntropyProviders::MockEntropyProviders(
    MockEntropyProviders::Results results,
    uint32_t low_entropy_domain)
    : EntropyProviders(results.high_entropy.has_value() ? "client_id" : "",
                       {0, low_entropy_domain},
                       results.limited_entropy.has_value()
                           ? "limited_entropy_randomization_source"
                           : std::string_view()),
      low_provider_(results.low_entropy),
      high_provider_(results.high_entropy.value_or(0)),
      limited_provider_(results.limited_entropy.value_or(0)) {}

MockEntropyProviders::~MockEntropyProviders() = default;

const base::FieldTrial::EntropyProvider& MockEntropyProviders::low_entropy()
    const {
  return low_provider_;
}

const base::FieldTrial::EntropyProvider& MockEntropyProviders::default_entropy()
    const {
  if (default_entropy_is_high_entropy()) {
    return high_provider_;
  }
  return low_provider_;
}

const base::FieldTrial::EntropyProvider& MockEntropyProviders::limited_entropy()
    const {
  CHECK(has_limited_entropy());
  return limited_provider_;
}

std::string GZipAndB64EncodeToHexString(const VariationsSeed& seed) {
  auto serialized = seed.SerializeAsString();
  std::string compressed;
  compression::GzipCompress(serialized, &compressed);
  return base::Base64Encode(compressed);
}

bool ContainsTrialName(const std::vector<ActiveGroupId>& active_group_ids,
                       std::string_view trial_name) {
  auto hashed_name = HashName(trial_name);
  for (const auto& trial : active_group_ids) {
    if (trial.name == hashed_name) {
      return true;
    }
  }
  return false;
}

bool ContainsTrialAndGroupName(
    const std::vector<ActiveGroupId>& active_group_ids,
    std::string_view trial_name,
    std::string_view group_name) {
  auto hashed_trial_name = HashName(trial_name);
  auto hashed_group_name = HashName(group_name);
  for (const auto& trial : active_group_ids) {
    if (trial.name == hashed_trial_name && trial.group == hashed_group_name) {
      return true;
    }
  }
  return false;
}

}  // namespace variations
