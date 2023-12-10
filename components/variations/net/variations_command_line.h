// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_NET_VARIATIONS_COMMAND_LINE_H_
#define COMPONENTS_VARIATIONS_NET_VARIATIONS_COMMAND_LINE_H_

#include <optional>
#include <string>

namespace base {
class CommandLine;
class FeatureList;
class FilePath;
}  // namespace base

namespace variations {

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
  std::string ToString();

  // Applies the state to the given `command_line`.
  void ApplyToCommandLine(base::CommandLine& command_line) const;

  // Applies the state to the current process' FeatureList and FieldTrialList.
  void ApplyToFeatureAndFieldTrialList(base::FeatureList* feature_list) const;

  // Loads the feature state as serialized by `WriteToFile` from `file_path`.
  // Returns nullopt if loading failed for some reason.
  static std::optional<VariationsCommandLine> ReadFromFile(
      const base::FilePath& file_path);

  // Serializes the state to `file_path`. The serialized state can be loaded
  // back in with `ReadFromFile`. Returns false if saving failed for some
  // reason.
  bool WriteToFile(const base::FilePath& file_path) const;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_NET_VARIATIONS_COMMAND_LINE_H_