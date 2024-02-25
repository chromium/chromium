// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TPCD_METADATA_PARSER_H_
#define COMPONENTS_TPCD_METADATA_PARSER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/metrics/field_trial_params.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/content_settings/core/common/content_settings_enums.mojom.h"
#include "components/tpcd/metadata/metadata.pb.h"

namespace tpcd::metadata {

using MetadataEntries = std::vector<MetadataEntry>;
using TpcdMetadataRuleSource = content_settings::mojom::TpcdMetadataRuleSource;

class Parser {
 public:
  static Parser* GetInstance();
  class Observer {
   public:
    virtual void OnMetadataReady() = 0;
  };

  Parser();
  virtual ~Parser();

  Parser(const Parser&) = delete;
  Parser& operator=(const Parser&) = delete;

  // Adds/Removes an Observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void CallOnMetadataReady();

  // ParseMetadata deserializes the proto content from `raw_metadata`.
  // NOTE: The validation of `raw_metadata` will be performed within the
  // ComponentInstaller's VerifyInstallation method before feeding it to this
  // method. So it is safe to assume this input is validated.
  void ParseMetadata(const std::string& raw_metadata);

  // GetMetadata returns an `std::vector` of `MetadataEntry`.
  // NOTE: Metadata from field trial params take precedence over the ones from
  // Component Updater.
  MetadataEntries GetMetadata();

  static constexpr char const* kMetadataFeatureParamName = "Metadata";
  static constexpr char const* kSourceUnspecified = "SOURCE_UNSPECIFIED";
  static constexpr char const* kSourceTest = "SOURCE_TEST";
  static constexpr char const* kSource1pDt = "SOURCE_1P_DT";
  static constexpr char const* kSource3pDt = "SOURCE_3P_DT";
  static constexpr char const* kSourceDogFood = "SOURCE_DOGFOOD";
  static constexpr char const* kSourceCriticalSector = "SOURCE_CRITICAL_SECTOR";
  static constexpr char const* kSourceCuj = "SOURCE_CUJ";
  static constexpr char const* kSourceGovEduTld = "SOURCE_GOV_EDU_TLD";

  // Converts the TPCD `MetadataEntry` `Source` field to its corresponding
  // `content_settings::RuleSource` enum value.
  static TpcdMetadataRuleSource ToRuleSource(const std::string& source);

  // Start Parser testing methods:
  MetadataEntries GetInstalledMetadataForTesting();
  void ResetStatesForTesting();
  MetadataEntries ParseMetadataFromFeatureParamForTesting(
      const base::FieldTrialParams& params);
  // End Parser testing methods.

 private:
  base::ObserverList<Observer>::Unchecked observers_;
  std::optional<MetadataEntries> metadata_
      GUARDED_BY_CONTEXT(sequence_checker_) = std::nullopt;

  SEQUENCE_CHECKER(sequence_checker_);
};
}  // namespace tpcd::metadata
#endif  // COMPONENTS_TPCD_METADATA_PARSER_H_
