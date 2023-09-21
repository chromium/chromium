// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tpcd/metadata/parser.h"

#include <string>
#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "components/tpcd/metadata/metadata.pb.h"
#include "components/tpcd/metadata/parser.h"
#include "net/base/features.h"
#include "third_party/zlib/google/compression_utils.h"

namespace tpcd::metadata {
// static
Parser* Parser::GetInstance() {
  static base::NoDestructor<Parser> instance;
  return instance.get();
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
    return ParseMetadataFromFeatureParam(params);
  }

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

}  // namespace tpcd::metadata
