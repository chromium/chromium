// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/core/about_this_site_validation.h"

#include "base/feature_list.h"
#include "components/page_info/core/features.h"
#include "components/page_info/core/proto/about_this_site_metadata.pb.h"
#include "url/gurl.h"

namespace page_info::about_this_site_validation {

AboutThisSiteStatus ValidateSource(const proto::Hyperlink& source) {
  if (!source.has_label())
    return AboutThisSiteStatus::kIncompleteSource;
  if (!source.has_url())
    return AboutThisSiteStatus::kIncompleteSource;

  GURL url(source.url());
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return AboutThisSiteStatus::kInvalidSource;
  }
  return AboutThisSiteStatus::kValid;
}

AboutThisSiteStatus ValidateDescription(
    const proto::SiteDescription& description) {
  if (!description.has_description())
    return AboutThisSiteStatus::kMissingDescriptionDescription;
  if (!description.has_name())
    return AboutThisSiteStatus::kMissingDescriptionName;
  if (!description.has_lang())
    return AboutThisSiteStatus::kMissingDescriptionLang;
  if (!description.has_source())
    return AboutThisSiteStatus::kMissingDescriptionSource;

  return ValidateSource(description.source());
}

AboutThisSiteStatus ValidateFirstSeen(const proto::SiteFirstSeen& first_seen) {
  if (!first_seen.has_count())
    return AboutThisSiteStatus::kIncompleteTimeStamp;
  if (!first_seen.has_unit())
    return AboutThisSiteStatus::kIncompleteTimeStamp;
  if (!first_seen.has_precision())
    return AboutThisSiteStatus::kIncompleteTimeStamp;

  if (first_seen.count() < 0)
    return AboutThisSiteStatus::kInvalidTimeStamp;
  if (first_seen.unit() == proto::UNIT_UNSPECIFIED)
    return AboutThisSiteStatus::kInvalidTimeStamp;
  if (first_seen.precision() == proto::PRECISION_UNSPECIFIED)
    return AboutThisSiteStatus::kInvalidTimeStamp;

  return AboutThisSiteStatus::kValid;
}

AboutThisSiteStatus ValidateMoreAbout(const proto::MoreAbout& more_info) {
  if (!more_info.has_url() || !GURL(more_info.url()).is_valid())
    return AboutThisSiteStatus::kInvalidMoreAbout;

  return AboutThisSiteStatus::kValid;
}

AboutThisSiteStatus ValidateSiteInfo(const proto::SiteInfo& site_info) {
  if (!site_info.has_description() && !site_info.has_first_seen() &&
      !site_info.has_more_about())
    return AboutThisSiteStatus::kEmptySiteInfo;

  AboutThisSiteStatus status = AboutThisSiteStatus::kValid;

  if (site_info.has_description()) {
    status = ValidateDescription(site_info.description());
  }

  if (status != AboutThisSiteStatus::kValid)
    return status;

  if (site_info.has_first_seen())
    status = ValidateFirstSeen(site_info.first_seen());

  if (status != AboutThisSiteStatus::kValid)
    return status;

  if (!site_info.has_more_about()) {
    return AboutThisSiteStatus::kMissingMoreAbout;
  }

  if (site_info.has_more_about())
    status = ValidateMoreAbout(site_info.more_about());

  return status;
}

AboutThisSiteStatus ValidateMetadata(
    const std::optional<proto::AboutThisSiteMetadata>& metadata) {
  if (!metadata)
    return AboutThisSiteStatus::kNoResult;
  if (!metadata->has_site_info())
    return AboutThisSiteStatus::kMissingSiteInfo;
  return ValidateSiteInfo(metadata->site_info());
}

}  // namespace page_info::about_this_site_validation
