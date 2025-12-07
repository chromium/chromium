// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/net/variations_command_line.h"

#include "base/base64.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/variations/field_trial_config/field_trial_util.h"
#include "components/variations/net/variations_command_line.h"
#include "components/variations/variations_switches.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "base/check_is_test.h"
#include "third_party/boringssl/src/include/openssl/hpke.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS)
// Prod key for feedback encryption.
// To debug the workflow with a dev key, or replace the prod key, please see
// google3/analysis/uma/tools/extract_public_key.py for more information.
const std::array<uint8_t, X25519_PUBLIC_VALUE_LEN> kFeedbackEncryptionPublicKey{
    0x21, 0x4f, 0x93, 0x34, 0x1f, 0x3a, 0xf8, 0xcb, 0x90, 0xd8, 0x13,
    0x4c, 0x42, 0x74, 0x77, 0x81, 0x1b, 0x68, 0x1e, 0xe8, 0xc3, 0x49,
    0x8b, 0x68, 0x10, 0x56, 0xb0, 0xf8, 0xc0, 0xd2, 0x61, 0x01};
#endif

// Exits the browser with a helpful error message.
void ExitWithMessage(const std::string& message) {
  UNSAFE_TODO(puts(message.c_str()));
  exit(1);
}

namespace variations {

#if !BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kFeedbackIncludeVariations, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

void MaybeUnpackVariationsStateFile() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(variations::switches::kVariationsStateFile)) {
    return;
  }

  // Do not allow mixing with other experiments flags.
  if (command_line->HasSwitch(::switches::kForceFieldTrials) ||
      command_line->HasSwitch(variations::switches::kForceFieldTrialParams) ||
      command_line->HasSwitch(::switches::kEnableFeatures) ||
      command_line->HasSwitch(::switches::kDisableFeatures)) {
    std::string msg = base::StringPrintf(
        "--%s can not work with other field-trial related flags:"
        " --%s, --%s, --%s, --%s",
        variations::switches::kVariationsStateFile,
        ::switches::kForceFieldTrials,
        variations::switches::kForceFieldTrialParams,
        ::switches::kEnableFeatures, ::switches::kDisableFeatures);
    ExitWithMessage(msg);
  }

  base::FilePath variations_path = command_line->GetSwitchValuePath(
      variations::switches::kVariationsStateFile);
  std::string file_content;
  bool success = base::ReadFileToString(variations_path, &file_content);
  if (!success) {
    ExitWithMessage(base::StrCat(
        {"Can not read from file: ", variations_path.AsUTF8Unsafe(),
         " defined in --", variations::switches::kVariationsStateFile}));
  }
  base::TrimString(file_content, base::kWhitespaceASCII, &file_content);
  std::string serialized_json;
  success = base::Base64Decode(file_content, &serialized_json);
  if (!success) {
    ExitWithMessage(base::StrCat(
        {"Base64 decode failed from file: ", variations_path.AsUTF8Unsafe(),
         " defined in --", variations::switches::kVariationsStateFile}));
  }

  auto optional_variations =
      variations::VariationsCommandLine::ReadFromString(serialized_json);
  if (!optional_variations.has_value()) {
    ExitWithMessage(
        base::StrCat({"File content may not be in json format: ",
                      variations_path.AsUTF8Unsafe(), " defined in --",
                      variations::switches::kVariationsStateFile}));
  }
  optional_variations->ApplyToCommandLine(*command_line);

  command_line->RemoveSwitch(variations::switches::kVariationsStateFile);
}

namespace {

// Format the provided |param_key| and |param_value| as commandline input.
std::string GenerateParam(const std::string& param_key,
                          const std::string& param_value) {
  if (!param_value.empty()) {
    return " --" + param_key + "=\"" + param_value + "\"";
  }

  return "";
}

std::string GetStringFromDict(const base::Value::Dict& dict,
                              std::string_view key) {
  const std::string* s = dict.FindString(key);
  return s ? *s : std::string();
}

#if !BUILDFLAG(IS_CHROMEOS)
// Encrypt `plaintext` with the `public_key` and save the result to
// `ciphertext`. Also if `enc_len` is not null, update the length of enc
// which is stored in `ciphertext`.
VariationsStateEncryptionStatus EncryptStringWithPublicKey(
    const std::string& plaintext,
    std::vector<uint8_t>* ciphertext,
    base::span<const uint8_t> public_key,
    size_t* enc_len = nullptr) {
  if (plaintext.empty()) {
    return VariationsStateEncryptionStatus::kEmptyInput;
  }
  bssl::ScopedEVP_HPKE_CTX sender_context;

  // The vector will hold the encapsulated shared secret "enc" followed by the
  // symmetrically encrypted ciphertext "ct". Start with a size big enough for
  // the shared secret.
  ciphertext->resize(EVP_HPKE_MAX_ENC_LENGTH);
  size_t encapsulated_shared_secret_len;

  if (!EVP_HPKE_CTX_setup_sender(
          /*ctx=*/sender_context.get(),
          /*out_enc=*/ciphertext->data(),
          /*out_enc_len=*/&encapsulated_shared_secret_len,
          /*max_enc=*/ciphertext->size(),
          /*kem=*/EVP_hpke_x25519_hkdf_sha256(),
          /*kdf=*/EVP_hpke_hkdf_sha256(),
          /*aead=*/EVP_hpke_aes_256_gcm(),
          /*peer_public_key=*/public_key.data(),
          /*peer_public_key_len=*/public_key.size(),
          /*info=*/nullptr,
          /*info_len=*/0)) {
    DVLOG(1) << "hpke setup failed";
    return VariationsStateEncryptionStatus::kHpkeSetupFailure;
  }
  if (enc_len != nullptr) {
    *enc_len = encapsulated_shared_secret_len;
  }
  // This vector holds encapsulated shared secret and encrypted text.
  // The encrypted text can be longer so we need to reserve enough length.
  ciphertext->resize(encapsulated_shared_secret_len + plaintext.length() +
                     EVP_HPKE_CTX_max_overhead(sender_context.get()));
  auto ciphertext_span =
      base::span(*ciphertext).subspan(encapsulated_shared_secret_len);
  size_t ciphertext_len;

  if (!EVP_HPKE_CTX_seal(
          /*ctx=*/sender_context.get(),
          /*out=*/ciphertext_span.data(),
          /*out_len=*/&ciphertext_len,
          /*max_out_len=*/ciphertext_span.size(),
          /*in=*/reinterpret_cast<const uint8_t*>(plaintext.c_str()),
          /*in_len=*/plaintext.length(),
          /*ad=*/nullptr,
          /*ad_len=*/0)) {
    DVLOG(1) << "hpke seal failed";
    return VariationsStateEncryptionStatus::kHpkeSealFailure;
  }
  ciphertext->resize(encapsulated_shared_secret_len + ciphertext_len);
  return VariationsStateEncryptionStatus::kSuccess;
}
#endif

}  // namespace

VariationsCommandLine::VariationsCommandLine() = default;
VariationsCommandLine::~VariationsCommandLine() = default;
VariationsCommandLine::VariationsCommandLine(VariationsCommandLine&&) = default;
VariationsCommandLine& VariationsCommandLine::operator=(
    VariationsCommandLine&&) = default;

VariationsCommandLine VariationsCommandLine::GetForCurrentProcess() {
  VariationsCommandLine result;
  base::FieldTrialList::AllStatesToString(&result.field_trial_states);
  result.field_trial_params =
      base::FieldTrialList::AllParamsToString(&EscapeValue);
  base::FeatureList::GetInstance()->GetFeatureOverrides(
      &result.enable_features, &result.disable_features);
  return result;
}

VariationsCommandLine VariationsCommandLine::GetForCommandLine(
    const base::CommandLine& command_line) {
  VariationsCommandLine result;
  result.field_trial_states =
      command_line.GetSwitchValueASCII(::switches::kForceFieldTrials);
  result.field_trial_params =
      command_line.GetSwitchValueASCII(switches::kForceFieldTrialParams);
  result.enable_features =
      command_line.GetSwitchValueASCII(::switches::kEnableFeatures);
  result.disable_features =
      command_line.GetSwitchValueASCII(::switches::kDisableFeatures);
  return result;
}

std::string VariationsCommandLine::ToString() const {
  std::string output;
  output.append(
      GenerateParam(::switches::kForceFieldTrials, field_trial_states));
  output.append(
      GenerateParam(switches::kForceFieldTrialParams, field_trial_params));
  output.append(GenerateParam(::switches::kEnableFeatures, enable_features));
  output.append(GenerateParam(::switches::kDisableFeatures, disable_features));
  output.append(" --");
  output.append(switches::kDisableFieldTrialTestingConfig);
  return output;
}

void VariationsCommandLine::ApplyToCommandLine(
    base::CommandLine& command_line) const {
  if (!field_trial_states.empty()) {
    command_line.AppendSwitchASCII(::switches::kForceFieldTrials,
                                   field_trial_states);
  }
  if (!field_trial_params.empty()) {
    command_line.AppendSwitchASCII(switches::kForceFieldTrialParams,
                                   field_trial_params);
  }
  if (!enable_features.empty()) {
    command_line.AppendSwitchASCII(::switches::kEnableFeatures,
                                   enable_features);
  }
  if (!disable_features.empty()) {
    command_line.AppendSwitchASCII(::switches::kDisableFeatures,
                                   disable_features);
  }
  command_line.AppendSwitch(switches::kDisableFieldTrialTestingConfig);
}

void VariationsCommandLine::ApplyToFeatureAndFieldTrialList(
    base::FeatureList* feature_list) const {
  if (!field_trial_params.empty()) {
    variations::AssociateParamsFromString(field_trial_params);
  }
  if (!field_trial_states.empty()) {
    base::FieldTrialList::CreateTrialsFromString(field_trial_states);
  }
  feature_list->InitFromCommandLine(enable_features, disable_features);
}

std::optional<VariationsCommandLine> VariationsCommandLine::ReadFromFile(
    const base::FilePath& file_path) {
  std::string content;
  bool success = base::ReadFileToString(file_path, &content);
  if (!success) {
    return std::nullopt;
  }
  return ReadFromString(content);
}

std::optional<VariationsCommandLine> VariationsCommandLine::ReadFromString(
    const std::string& serialized_json) {
  JSONStringValueDeserializer deserializer(serialized_json);
  std::unique_ptr<base::Value> value = deserializer.Deserialize(
      /*error_code=*/nullptr, /*error_message=*/nullptr);
  if (!value) {
    return std::nullopt;
  }
  base::Value::Dict* dict = value->GetIfDict();
  if (!dict) {
    return std::nullopt;
  }
  VariationsCommandLine result;
  result.field_trial_states =
      GetStringFromDict(*dict, ::switches::kForceFieldTrials);
  result.field_trial_params =
      GetStringFromDict(*dict, switches::kForceFieldTrialParams);
  result.enable_features =
      GetStringFromDict(*dict, ::switches::kEnableFeatures);
  result.disable_features =
      GetStringFromDict(*dict, ::switches::kDisableFeatures);
  return result;
}

bool VariationsCommandLine::WriteToFile(const base::FilePath& file_path) const {
  std::string content;
  bool success = WriteToString(&content);
  if (!success) {
    return false;
  }
  return base::WriteFile(file_path, content);
}

bool VariationsCommandLine::WriteToString(std::string* serialized_json) const {
  base::Value::Dict dict =
      base::Value::Dict()
          .Set(::switches::kForceFieldTrials, field_trial_states)
          .Set(switches::kForceFieldTrialParams, field_trial_params)
          .Set(::switches::kEnableFeatures, enable_features)
          .Set(::switches::kDisableFeatures, disable_features);
  JSONStringValueSerializer serializer(serialized_json);
  return serializer.Serialize(dict);
}

#if !BUILDFLAG(IS_CHROMEOS)
VariationsStateEncryptionStatus VariationsCommandLine::EncryptToString(
    std::vector<uint8_t>* ciphertext) const {
  return EncryptStringWithPublicKey(ToString(), ciphertext,
                                    kFeedbackEncryptionPublicKey);
}

VariationsStateEncryptionStatus
VariationsCommandLine::EncryptToStringForTesting(
    std::vector<uint8_t>* ciphertext,
    base::span<const uint8_t> public_key,
    size_t* enc_len) const {
  CHECK_IS_TEST();
  return EncryptStringWithPublicKey(ToString(), ciphertext, public_key,
                                    enc_len);
}
#endif

}  // namespace variations
