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

// The compressed data is the result of decoding the base64 encoded compressed
// data above and showing the data as hex:
// echo -n base64_compressed_data | base64 -d | hexdump -e '8 1 ", 0x%x"'
const uint8_t kTestSeed_CompressedData[] = {
    0x1f, 0x8b, 0x8,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0xff, 0xe2, 0x62,
    0x29, 0x49, 0x2d, 0x2e, 0x11, 0xb2, 0xe3, 0x92, 0xf,  0xf5, 0x75, 0xd4,
    0xd,  0xcd, 0xcb, 0x4c, 0xcb, 0x2f, 0xca, 0xcd, 0x2c, 0xa9, 0xd4, 0xd,
    0x29, 0xca, 0x4c, 0xcc, 0xd1, 0x35, 0x35, 0xd0, 0xd,  0x48, 0x2d, 0x4a,
    0x4e, 0xcd, 0x2b, 0xb1, 0x60, 0xf4, 0xe2, 0xe6, 0x62, 0x4f, 0x49, 0x4d,
    0x4b, 0x2c, 0xcd, 0x29, 0x11, 0x60, 0xf4, 0xe2, 0xe1, 0xe2, 0x48, 0x2f,
    0xca, 0x2f, 0x2d, 0x88, 0x37, 0x30, 0x14, 0x60, 0x4,  0x4,  0x0,  0x0,
    0xff, 0xff, 0x74, 0xfc, 0x98, 0x4,  0x46, 0x0,  0x0,  0x0};

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

// The compressed data is the result of decoding the base64 encoded compressed
// data above and showing the data as hex:
// echo -n base64_compressed_data | base64 -d | hexdump -e '8 1 ", 0x%x"'
const uint8_t kCrashingSeed_CompressedData[] = {
    0x1f, 0x8b, 0x8,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x8d, 0xd0,
    0xc1, 0x4b, 0xc3, 0x30, 0x14, 0xc7, 0xf1, 0xb6, 0x9b, 0xc2, 0x2,  0x83,
    0xb1, 0xa3, 0x7,  0x29, 0x93, 0x41, 0x10, 0xa7, 0x49, 0x93, 0xa6, 0xc9,
    0x59, 0xb6, 0xc3, 0x10, 0x4,  0x37, 0xd0, 0x9b, 0x24, 0x79, 0x2f, 0xb6,
    0x50, 0xaa, 0x74, 0xed, 0xc1, 0x7f, 0x66, 0x7f, 0xab, 0x65, 0xf8, 0x7,
    0xf4, 0xfc, 0xbe, 0xef, 0x73, 0xf8, 0x11, 0x2a, 0x72, 0x84, 0xc,  0xc,
    0x8a, 0x5c, 0x3a, 0xc9, 0xa5, 0xc3, 0x0,  0xc1, 0x8,  0x66, 0xb,  0x21,
    0x99, 0x91, 0x8c, 0x1b, 0xcf, 0xb8, 0xca, 0x2,  0x5f, 0x9e, 0x13, 0x32,
    0x7f, 0x6e, 0xed, 0xa9, 0xac, 0x9a, 0xaf, 0x43, 0xd7, 0xc3, 0xaf, 0x8e,
    0xf7, 0x9a, 0xcc, 0xb7, 0x8d, 0x75, 0x35, 0xc2, 0x8b, 0xed, 0x1b, 0x5f,
    0x2e, 0xc0, 0xad, 0xc9, 0xdd, 0xee, 0xbb, 0xf5, 0xb8, 0xab, 0xb0, 0x86,
    0x63, 0x5b, 0xd9, 0xfa, 0x80, 0x5d, 0xff, 0x73, 0xf9, 0x1c, 0xe,  0x47,
    0x3c, 0x75, 0x3,  0xb0, 0x7f, 0x27, 0xf,  0x97, 0xc,  0x5e, 0x9b, 0xcf,
    0x11, 0xfd, 0x22, 0x72, 0xeb, 0x9b, 0x51, 0xf0, 0x7,  0xd9, 0xfc, 0xc3,
    0x21, 0x8c, 0x95, 0x57, 0x63, 0xe4, 0xb7, 0xdb, 0xe5, 0xd4, 0xf0, 0xc7,
    0xfb, 0x34, 0x4a, 0xe3, 0x34, 0x49, 0x27, 0x74, 0x4a, 0xaf, 0xe8, 0x35,
    0x8d, 0x68, 0x4c, 0x13, 0x3a, 0xa1, 0xb3, 0xd5, 0xa6, 0x1c, 0xfa, 0x27,
    0x69, 0x6d, 0xae, 0x2c, 0x7,  0x2f, 0x18, 0x4,  0x5f, 0xa8, 0x42, 0xf1,
    0x3c, 0x93, 0x1a, 0x54, 0x40, 0xcc, 0x8c, 0x16, 0xc3, 0xa2, 0xda, 0x65,
    0x85, 0xfa, 0x3,  0xb3, 0xd3, 0x4b, 0x6e, 0x7a, 0x1,  0x0,  0x0};

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
     /*disable_benchmarking=*/std::nullopt,
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
    kTestSeed_StudyNames,           kTestSeed_Base64UncompressedData,
    kTestSeed_Base64CompressedData, kTestSeed_Base64Signature,
    kTestSeed_CompressedData,       sizeof(kTestSeed_CompressedData)};

const SignedSeedData kCrashingSeedData{
    kCrashingSeed_StudyNames,           kCrashingSeed_Base64UncompressedData,
    kCrashingSeed_Base64CompressedData, kCrashingSeed_Base64Signature,
    kCrashingSeed_CompressedData,       sizeof(kCrashingSeed_CompressedData)};

const SignedSeedPrefKeys kSafeSeedPrefKeys{prefs::kVariationsSafeCompressedSeed,
                                           prefs::kVariationsSafeSeedSignature};

const SignedSeedPrefKeys kRegularSeedPrefKeys{prefs::kVariationsCompressedSeed,
                                              prefs::kVariationsSeedSignature};

SignedSeedData::SignedSeedData(base::span<const char*> in_study_names,
                               const char* in_base64_uncompressed_data,
                               const char* in_base64_compressed_data,
                               const char* in_base64_signature,
                               const uint8_t* in_compressed_data,
                               size_t in_compressed_data_size)
    : study_names(std::move(in_study_names)),
      base64_uncompressed_data(in_base64_uncompressed_data),
      base64_compressed_data(in_base64_compressed_data),
      base64_signature(in_base64_signature),
      compressed_data(in_compressed_data),
      compressed_data_size(in_compressed_data_size) {}

SignedSeedData::~SignedSeedData() = default;

SignedSeedData::SignedSeedData(const SignedSeedData&) = default;
SignedSeedData::SignedSeedData(SignedSeedData&&) = default;
SignedSeedData& SignedSeedData::operator=(const SignedSeedData&) = default;
SignedSeedData& SignedSeedData::operator=(SignedSeedData&&) = default;

std::string_view SignedSeedData::GetCompressedData() const {
  return std::string_view(reinterpret_cast<const char*>(compressed_data),
                     compressed_data_size);
}

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
  if (!base::Base64Decode(variations, &serialized_proto)) {
    return false;
  }
  ClientVariations proto;
  if (!proto.ParseFromString(serialized_proto)) {
    return false;
  }
  for (int i = 0; i < proto.variation_id_size(); ++i) {
    variation_ids->insert(proto.variation_id(i));
  }
  for (int i = 0; i < proto.trigger_variation_id_size(); ++i) {
    trigger_ids->insert(proto.trigger_variation_id(i));
  }
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
  return std::ranges::all_of(seed_data.study_names, [](const char* study) {
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

void SetUpSeedFileTrial(std::string group_name) {
  if (group_name.empty()) {
    return;
  }
  base::MockEntropyProvider entropy_provider(0.9);
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          kSeedFileTrial, /*total_probability=*/100, kDefaultGroup,
          entropy_provider));

  trial->AppendGroup(group_name, /*group_probability=*/100);
  trial->SetForced();
}

}  // namespace variations
