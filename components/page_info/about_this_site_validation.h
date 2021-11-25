// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_INFO_ABOUT_THIS_SITE_VALIDATION_H_
#define COMPONENTS_PAGE_INFO_ABOUT_THIS_SITE_VALIDATION_H_

#include "components/page_info/proto/about_this_site_metadata.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace page_info {
namespace about_this_site_validation {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Keep in sync with AboutThisSiteStatus in enums.xml
enum class ProtoValidation {
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

  kMaxValue = kMissingDescriptionSource,
};

ProtoValidation ValidateMetadata(
    const absl::optional<proto::AboutThisSiteMetadata>& metadata);

ProtoValidation ValidateSource(const proto::Hyperlink& link);
ProtoValidation ValidateDescription(const proto::SiteDescription& description);
ProtoValidation ValidateFirstSeen(const proto::SiteFirstSeen& first_seen);
ProtoValidation ValidateSiteInfo(const proto::SiteInfo& site_info);

}  // namespace about_this_site_validation
}  // namespace page_info

#endif  // COMPONENTS_PAGE_INFO_ABOUT_THIS_SITE_VALIDATION_H_
