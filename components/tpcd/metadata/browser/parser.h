// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TPCD_METADATA_BROWSER_PARSER_H_
#define COMPONENTS_TPCD_METADATA_BROWSER_PARSER_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/metrics/field_trial_params.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/content_settings/core/common/content_settings_enums.mojom.h"
#include "components/tpcd/metadata/common/proto/metadata.pb.h"

namespace tpcd::metadata {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// NOTE: Keep in sync with `TpcdMetadataInstallationResult` at
// src/tools/metrics/histograms/metadata/navigation/enums.xml
enum class InstallationResult {
  // The metadata component was successfully .
  kSuccessful = 0,
  // The component file wasn't present.
  kMissingMetadataFile = 1,
  // Reading from the component file failed.
  kReadingMetadataFileFailed = 2,
  // The raw metadata string was unable to be parsed into the proto.
  kParsingToProtoFailed = 3,
  // One or more of the specs are erroneous or missing.
  kErroneousSpec = 4,
  // The Source field is erroneous or missing.
  kErroneousSource = 5,
  // The DTRP or its override field is erroneous or missing.
  kErroneousDtrp = 6,
  // The DTRP or its override field shouldn't be set.
  kIllicitDtrp = 7,
  kMaxValue = kIllicitDtrp,
};

// Enumerates the source of the `MetadataEntry` list return by `GetMetadata()`.
enum class MetadataSource {
  kServer = 0,
  kClient,
  kFeatureParams,
};

using RecordInstallationResultCallback =
    base::OnceCallback<void(InstallationResult)>;

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
  inline static bool IsTestEntry(const MetadataEntry& metadata_entry) {
    return metadata_entry.source() == kSourceTest;
  }
  static constexpr char const* kSource1pDt = "SOURCE_1P_DT";
  static constexpr char const* kSource3pDt = "SOURCE_3P_DT";
  static constexpr char const* kSourceDogFood = "SOURCE_DOGFOOD";
  static constexpr char const* kSourceCriticalSector = "SOURCE_CRITICAL_SECTOR";
  static constexpr char const* kSourceCuj = "SOURCE_CUJ";
  static constexpr char const* kSourceGovEduTld = "SOURCE_GOV_EDU_TLD";

  static const uint32_t kMinDtrp = 0;
  static const uint32_t kMaxDtrp = 100;
  static inline bool IsValidDtrp(const uint32_t dtrp) {
    return kMinDtrp <= dtrp && dtrp <= kMaxDtrp;
  }

  // Converts the TPCD `MetadataEntry` `Source` field to its corresponding
  // `content_settings::RuleSource` enum value.
  static TpcdMetadataRuleSource ToRuleSource(const std::string& source);

  static bool IsValidMetadata(
      const Metadata& metadata,
      RecordInstallationResultCallback callback = base::NullCallback());

  // Start Parser testing methods:
  MetadataEntries GetInstalledMetadataForTesting();
  void ResetStatesForTesting();
  MetadataEntries ParseMetadataFromFeatureParamForTesting(
      const base::FieldTrialParams& params);
  // End Parser testing methods.

  MetadataSource get_metadata_source() { return metadata_source_; }

 private:
  base::ObserverList<Observer>::Unchecked observers_;
  std::optional<MetadataEntries> metadata_
      GUARDED_BY_CONTEXT(sequence_checker_) = std::nullopt;
  MetadataSource metadata_source_ = MetadataSource::kServer;

  SEQUENCE_CHECKER(sequence_checker_);
};

namespace helpers {
MetadataEntry* AddEntryToMetadata(
    Metadata& metadata,
    const std::string& primary_pattern_spec,
    const std::string& secondary_pattern_spec,
    const std::string& source = Parser::kSourceTest,
    const std::optional<uint32_t>& dtrp = std::nullopt,
    const std::optional<uint32_t>& dtrp_override = std::nullopt);
}  // namespace helpers
}  // namespace tpcd::metadata
#endif  // COMPONENTS_TPCD_METADATA_BROWSER_PARSER_H_
