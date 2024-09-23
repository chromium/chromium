// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tpcd/metadata/browser/parser.h"

#include <string>
#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/features.h"
#include "components/tpcd/metadata/common/proto/metadata.pb.h"
#include "net/base/features.h"
#include "third_party/zlib/google/compression_utils.h"

namespace tpcd::metadata {
// static
Parser* Parser::GetInstance() {
  static base::NoDestructor<Parser> instance;
  return instance.get();
}

// static
TpcdMetadataRuleSource Parser::ToRuleSource(const std::string& source) {
  if (source == kSourceTest) {
    return TpcdMetadataRuleSource::SOURCE_TEST;
  } else if (source == kSource1pDt) {
    return TpcdMetadataRuleSource::SOURCE_1P_DT;
  } else if (source == kSource3pDt) {
    return TpcdMetadataRuleSource::SOURCE_3P_DT;
  } else if (source == kSourceDogFood) {
    return TpcdMetadataRuleSource::SOURCE_DOGFOOD;
  } else if (source == kSourceCriticalSector) {
    return TpcdMetadataRuleSource::SOURCE_CRITICAL_SECTOR;
  } else if (source == kSourceCuj) {
    return TpcdMetadataRuleSource::SOURCE_CUJ;
  } else if (source == kSourceGovEduTld) {
    return TpcdMetadataRuleSource::SOURCE_GOV_EDU_TLD;
  }

  // `SOURCE_UNSPECIFIED` is never send by the server. It is considered
  // invalid by the sanitizer. Thus, used here as a translation for any new,
  // uncategorized server source type.
  return TpcdMetadataRuleSource::SOURCE_UNSPECIFIED;
}

// static
bool Parser::IsValidMetadata(const Metadata& metadata,
                             RecordInstallationResultCallback callback) {
  for (const tpcd::metadata::MetadataEntry& me : metadata.metadata_entries()) {
    if (!me.has_primary_pattern_spec() ||
        !ContentSettingsPattern::FromString(me.primary_pattern_spec())
             .IsValid()) {
      if (callback) {
        std::move(callback).Run(InstallationResult::kErroneousSpec);
      }
      return false;
    }

    if (!me.has_secondary_pattern_spec() ||
        !ContentSettingsPattern::FromString(me.secondary_pattern_spec())
             .IsValid()) {
      if (callback) {
        std::move(callback).Run(InstallationResult::kErroneousSpec);
      }
      return false;
    }

    if (!me.has_source()) {
      if (callback) {
        std::move(callback).Run(InstallationResult::kErroneousSource);
      }
      return false;
    }

    if (base::FeatureList::IsEnabled(
            net::features::kTpcdMetadataStageControl)) {
      if (me.has_dtrp() && !IsValidDtrp(me.dtrp())) {
        if (callback) {
          std::move(callback).Run(InstallationResult::kErroneousDtrp);
        }
        return false;
      } else if (me.has_dtrp_override() &&
                 (!me.has_dtrp() || !IsValidDtrp(me.dtrp_override()))) {
        if (callback) {
          std::move(callback).Run(InstallationResult::kErroneousDtrp);
        }
        return false;
      }
    }
  }

  return true;
}

Parser::Parser() = default;
Parser::~Parser() = default;

MetadataEntries ToMetadataEntries(const Metadata& metadata) {
  MetadataEntries metadata_entries;
  for (auto me : metadata.metadata_entries()) {
    metadata_entries.emplace_back(me);
  }
  return metadata_entries;
}

void Parser::ParseMetadata(const std::string& raw_metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Metadata metadata;
  // Not expected to ever fail when called by the component updater as
  // the `TpcdMetadataComponentInstaller::VerifyInstallation()` already makes
  // sure of it.
  CHECK(metadata.ParseFromString(raw_metadata));

  metadata_ = ToMetadataEntries(metadata);

  CallOnMetadataReady();
}

MetadataEntries ParseMetadataFromFeatureParam(
    const base::FieldTrialParams& params) {
  Metadata metadata;

  std::string raw_metadata;
  CHECK(base::Base64Decode(
      params.find(Parser::kMetadataFeatureParamName)->second, &raw_metadata));

  std::string uncompressed;
  CHECK(compression::GzipUncompress(raw_metadata, &uncompressed));

  CHECK(metadata.ParseFromString(uncompressed));

  CHECK(Parser::IsValidMetadata(metadata, base::NullCallback()));

  return ToMetadataEntries(metadata);
}

void Parser::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void Parser::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void Parser::CallOnMetadataReady() {
  for (auto& observer : observers_) {
    observer.OnMetadataReady();
  }
}

MetadataEntries Parser::GetMetadata() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::FieldTrialParams params;
  bool has_feature_params = base::GetFieldTrialParamsByFeature(
      net::features::kTpcdMetadataGrants, &params);
  if (has_feature_params &&
      params.find(kMetadataFeatureParamName) != params.end()) {
    metadata_source_ = MetadataSource::kFeatureParams;
    return ParseMetadataFromFeatureParam(params);
  }

  metadata_source_ = MetadataSource::kServer;
  // If no metadata are present within the Feature params, use the metadata
  // provided by the Component Updater if present.
  return metadata_.value_or(MetadataEntries());
}

// Start Parser testing methods impl:
MetadataEntries Parser::GetInstalledMetadataForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return metadata_.value_or(MetadataEntries());
}

void Parser::ResetStatesForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  metadata_.reset();
}

MetadataEntries Parser::ParseMetadataFromFeatureParamForTesting(
    const base::FieldTrialParams& params) {
  return ParseMetadataFromFeatureParam(params);
}
// End Parser testing methods impl.

namespace helpers {
MetadataEntry* AddEntryToMetadata(
    Metadata& metadata,
    const std::string& primary_pattern_spec,
    const std::string& secondary_pattern_spec,
    const std::string& source,
    const std::optional<uint32_t>& dtrp,
    const std::optional<uint32_t>& dtrp_override) {
  MetadataEntry* me = metadata.add_metadata_entries();
  me->set_primary_pattern_spec(primary_pattern_spec);
  me->set_secondary_pattern_spec(secondary_pattern_spec);
  me->set_source(source);
  if (dtrp.has_value()) {
    me->set_dtrp(dtrp.value());
  }
  if (dtrp_override.has_value()) {
    me->set_dtrp_override(dtrp_override.value());
  }

  DCHECK(Parser::IsValidMetadata(metadata));

  return me;
}
}  // namespace helpers
}  // namespace tpcd::metadata
