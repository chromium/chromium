// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TPCD_METADATA_PARSER_H_
#define COMPONENTS_TPCD_METADATA_PARSER_H_

#include <string>
#include <vector>

#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/tpcd/metadata/metadata.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace tpcd::metadata {

using MetadataEntries = std::vector<MetadataEntry>;

class Parser {
 public:
  static Parser* GetInstance();

  Parser(const Parser&) = delete;
  Parser& operator=(const Parser&) = delete;

  // ParseMetadata deserializes the proto content from `raw_metadata`.
  // NOTE: The validation of `raw_metadata` will be performed within the
  // ComponentInstaller's VerifyInstallation method before feeding it to this
  // method. So it is safe to assume this input is validated.
  void ParseMetadata(const std::string& raw_metadata);

  // GetMetadata returns an `std::vector` of `MetadataEntry`.
  // NOTE: Metadata from field trial params take precedence over the ones from
  // Component Updater.
  MetadataEntries GetMetadata();

  MetadataEntries GetMetadataForTesting();
  void ResetStatesForTesting();
  MetadataEntries ParseMetadataFromFeatureParamForTesting(
      const base::FieldTrialParams& params);

  static constexpr char const* kMetadataFeatureParamName = "Metadata";

 private:
  friend class base::NoDestructor<Parser>;

  Parser();
  ~Parser();

  absl::optional<MetadataEntries> metadata_
      GUARDED_BY_CONTEXT(sequence_checker_) = absl::nullopt;

  SEQUENCE_CHECKER(sequence_checker_);
};
}  // namespace tpcd::metadata
#endif  // COMPONENTS_TPCD_METADATA_PARSER_H_
