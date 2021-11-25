// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/about_this_site_validation.h"

#include "components/page_info/proto/about_this_site_metadata.pb.h"
#include "url/gurl.h"

namespace page_info {
namespace about_this_site_validation {

ProtoValidation ValidateSource(const proto::Hyperlink& source) {
  if (!source.has_label())
    return ProtoValidation::kIncompleteSource;
  if (!source.has_url())
    return ProtoValidation::kIncompleteSource;

  GURL url(source.url());
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return ProtoValidation::kInvalidSource;
  }
  return ProtoValidation::kValid;
}

ProtoValidation ValidateDescription(const proto::SiteDescription& description) {
  if (!description.has_description())
    return ProtoValidation::kMissingDescriptionDescription;
  if (!description.has_name())
    return ProtoValidation::kMissingDescriptionName;
  if (!description.has_lang())
    return ProtoValidation::kMissingDescriptionLang;
  if (!description.has_source())
    return ProtoValidation::kMissingDescriptionSource;

  return ValidateSource(description.source());
}

ProtoValidation ValidateFirstSeen(const proto::SiteFirstSeen& first_seen) {
  if (!first_seen.has_count())
    return ProtoValidation::kIncompleteTimeStamp;
  if (!first_seen.has_unit())
    return ProtoValidation::kIncompleteTimeStamp;
  if (!first_seen.has_precision())
    return ProtoValidation::kIncompleteTimeStamp;

  if (first_seen.count() < 0)
    return ProtoValidation::kInvalidTimeStamp;
  if (first_seen.unit() == proto::UNIT_UNSPECIFIED)
    return ProtoValidation::kInvalidTimeStamp;
  if (first_seen.precision() == proto::PRECISION_UNSPECIFIED)
    return ProtoValidation::kInvalidTimeStamp;

  return ProtoValidation::kValid;
}

ProtoValidation ValidateSiteInfo(const proto::SiteInfo& site_info) {
  if (!site_info.has_description() && !site_info.has_first_seen())
    return ProtoValidation::kEmptySiteInfo;

  ProtoValidation status = ProtoValidation::kValid;
  if (!site_info.has_description())
    return ProtoValidation::kMissingDescription;
  status = ValidateDescription(site_info.description());
  if (status != ProtoValidation::kValid)
    return status;

  if (site_info.has_first_seen())
    status = ValidateFirstSeen(site_info.first_seen());
  return status;
}

ProtoValidation ValidateMetadata(
    const absl::optional<proto::AboutThisSiteMetadata>& metadata) {
  if (!metadata)
    return ProtoValidation::kNoResult;
  if (!metadata->has_site_info())
    return ProtoValidation::kMissingSiteInfo;
  return ValidateSiteInfo(metadata->site_info());
}

}  // namespace about_this_site_validation
}  // namespace page_info
