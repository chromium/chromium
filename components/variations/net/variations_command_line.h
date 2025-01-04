// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_NET_VARIATIONS_COMMAND_LINE_H_
#define COMPONENTS_VARIATIONS_NET_VARIATIONS_COMMAND_LINE_H_

#include <optional>
#include <string>

#include "build/build_config.h"
#include "build/buildflag.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "base/feature_list.h"
#endif

namespace base {
class CommandLine;
class FeatureList;
class FilePath;
}  // namespace base

namespace variations {

#if !BUILDFLAG(IS_CHROMEOS)
BASE_DECLARE_FEATURE(kFeedbackIncludeVariations);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(VariationsStateEncryptionStatus)
enum class VariationsStateEncryptionStatus {
  kSuccess = 0,
  kEmptyInput = 1,
  kHpkeSetupFailure = 2,
  kHpkeSealFailure = 3,
  kMaxValue = kHpkeSealFailure,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/variations/enums.xml:VariationsStateEncryptionStatus)
#endif

// Parses the content of `variations::switches::kVariationsStateFile` and
// modifies the command-line arguments of the running process by setting the
// switches contained in that file. The function will exit the process with
// an error message if the passed file's contents are invalid.
void MaybeUnpackVariationsStateFile();

// This struct contains all the fields that are needed to replicate the complete
// state of variations (including all the registered trials with corresponding
// groups, params and features) for the client, with methods to convert this to
// various formats.
struct VariationsCommandLine {
  std::string field_trial_states;
  std::string field_trial_params;
  std::string enable_features;
  std::string disable_features;

  VariationsCommandLine();
  ~VariationsCommandLine();
  VariationsCommandLine(VariationsCommandLine&&);
  VariationsCommandLine& operator=(VariationsCommandLine&&);

  // Creates a VariationsCommandLine for the current state of this client.
  static VariationsCommandLine GetForCurrentProcess();

  // Creates a VariationsCommandLine from the given `command_line`.
  static VariationsCommandLine GetForCommandLine(
      const base::CommandLine& command_line);

  // Returns the state in command-line format.
  std::string ToString() const;

  // Applies the state to the given `command_line`.
  void ApplyToCommandLine(base::CommandLine& command_line) const;

  // Applies the state to the current process' FeatureList and FieldTrialList.
  void ApplyToFeatureAndFieldTrialList(base::FeatureList* feature_list) const;

  // Loads the feature state as serialized by `WriteToFile` from `file_path`.
  // Returns nullopt if loading failed for some reason.
  static std::optional<VariationsCommandLine> ReadFromFile(
      const base::FilePath& file_path);

  // Loads the feature state as serialized by `WriteToString`.
  // Returns nullopt if loading failed for some reason.
  static std::optional<VariationsCommandLine> ReadFromString(
      const std::string& serialized_json);

  // Serializes the state to `file_path`. The serialized state can be loaded
  // back in with `ReadFromFile`. Returns false if saving failed for some
  // reason.
  bool WriteToFile(const base::FilePath& file_path) const;

  // Serializes the state to `serialized_json`. The serialized state can be
  // loaded back in with `ReadFromString`. Returns false if saving failed for
  // some reason.
  bool WriteToString(std::string* serialized_json) const;

#if !BUILDFLAG(IS_CHROMEOS)
  // Serializes and encrypts the state to `ciphertext` with a public key.
  // Encryption is needed for security and privacy purpose. This is used by
  // the feedback component.
  // The `ciphertext` is a combination of:
  //   The encapsulated shared secret + encrypted bytes.
  VariationsStateEncryptionStatus EncryptToString(
      std::vector<uint8_t>* ciphertext) const;

  // Test the internal function `EncryptStringWithPublicKey`.
  // The `ciphertext` is a combination of:
  //   The encapsulated shared secret + encrypted bytes.
  // If `enc_len` is not null, it will store the length of the
  // encapsulated shared secret.
  VariationsStateEncryptionStatus EncryptToStringForTesting(
      std::vector<uint8_t>* ciphertext,
      base::span<const uint8_t> public_key,
      size_t* enc_len) const;
#endif
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_NET_VARIATIONS_COMMAND_LINE_H_