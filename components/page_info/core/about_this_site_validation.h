// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_INFO_CORE_ABOUT_THIS_SITE_VALIDATION_H_
#define COMPONENTS_PAGE_INFO_CORE_ABOUT_THIS_SITE_VALIDATION_H_

#include "components/page_info/core/proto/about_this_site_metadata.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace page_info {
namespace about_this_site_validation {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Keep in sync with AboutThisSiteStatus in enums.xml
enum class AboutThisSiteStatus {
  kValid = 0,
  kNoResult = 1,
  kMissingSiteInfo = 2,
  kEmptySiteInfo = 3,
  // kIncompleteDescription = 4 deprecated.
  kIncompleteSource = 5,
  kInvalidSource = 6,
  kIncompleteTimeStamp = 7,
  kInvalidTimeStamp = 8,
  kUnknown = 9,
  kMissingDescription = 10,
  kMissingDescriptionDescription = 11,
  kMissingDescriptionName = 12,
  kMissingDescriptionLang = 13,
  kMissingDescriptionSource = 14,
  // Deprecated: kMissingBannerInfo = 15,
  kInvalidMoreAbout = 16,
  kMissingMoreAbout = 17,

  kMaxValue = kMissingMoreAbout,
};

AboutThisSiteStatus ValidateMetadata(
    const absl::optional<proto::AboutThisSiteMetadata>& metadata,
    bool allow_missing_description);

AboutThisSiteStatus ValidateSource(const proto::Hyperlink& link);
AboutThisSiteStatus ValidateDescription(
    const proto::SiteDescription& description);
AboutThisSiteStatus ValidateFirstSeen(const proto::SiteFirstSeen& first_seen);
AboutThisSiteStatus ValidateMoreAbout(const proto::MoreAbout& more_about);
AboutThisSiteStatus ValidateSiteInfo(const proto::SiteInfo& site_info,
                                     bool allow_missing_description);

}  // namespace about_this_site_validation
}  // namespace page_info

#endif  // COMPONENTS_PAGE_INFO_CORE_ABOUT_THIS_SITE_VALIDATION_H_
