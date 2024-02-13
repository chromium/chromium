// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_FAMILY_LINK_USER_LOG_RECORD_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_FAMILY_LINK_USER_LOG_RECORD_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"

namespace signin {
class IdentityManager;
}

namespace supervised_user {
class SupervisedUserURLFilter;

// Stores information required to log UMA record histograms for a FamilyLink
// user account.
class FamilyLinkUserLogRecord {
 public:
  // These enum values represent the user's supervision type and how the
  // supervision has been enabled.
  // These values are logged to UMA. Entries should not be renumbered and
  // numeric values should never be reused. Please keep in sync with
  // "FamilyLinkUserLogSegment" in src/tools/metrics/histograms/enums.xml.
  enum class Segment {
    // User is not supervised by FamilyLink.
    kUnsupervised = 0,
    // User that is required to be supervised by FamilyLink due to child account
    // policies (maps to Unicorn and Griffin accounts).
    kSupervisionEnabledByPolicy = 1,
    // User that has chosen to be supervised by FamilyLink (maps to Geller
    // accounts).
    kSupervisionEnabledByUser = 2,
    // Profile contains users with multiple different supervision status
    // used only when ExtendFamilyLinkUserLogSegmentToAllPlatforms flag is
    // enabled
    kMixedProfile = 3,
    // Add future entries above this comment, in sync with
    // "FamilyLinkUserLogSegment" in src/tools/metrics/histograms/enums.xml.
    // Update kMaxValue to the last value.
    kMaxValue = kMixedProfile
  };

  // Returns an immutable FamilyLinkUserLogRecord.
  static FamilyLinkUserLogRecord Create(
      signin::IdentityManager* identity_manager,
      SupervisedUserURLFilter* supervised_user_filter);

  // Returns the supervision status of the primary account.
  std::optional<Segment> GetSupervisionStatusForPrimaryAccount() const;

  // Returns the web filter applied to the account if it is supervised,
  // otherwise returns nullopt.
  std::optional<WebFilterType> GetWebFilterTypeForPrimaryAccount() const;

 private:
  FamilyLinkUserLogRecord(std::optional<Segment> supervision_status,
                          std::optional<WebFilterType> web_filter_type);

  std::optional<Segment> supervision_status_;
  std::optional<WebFilterType> web_filter_type_;
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_FAMILY_LINK_USER_LOG_RECORD_H_
