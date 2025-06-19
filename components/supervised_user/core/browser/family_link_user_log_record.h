// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_FAMILY_LINK_USER_LOG_RECORD_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_FAMILY_LINK_USER_LOG_RECORD_H_

#include <optional>
#include <ostream>

#include "base/memory/raw_ptr.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"

class HostContentSettingsMap;
class PrefService;

namespace signin {
class IdentityManager;
}

namespace supervised_user {
class SupervisedUserService;

// Stores information required to log UMA record histograms for a FamilyLink
// user account.
// TODO(crbug.com/425685013): Rename to SupervisedUserLogRecord.
class FamilyLinkUserLogRecord {
 public:
  // These enum values represent the user's supervision type and how the
  // supervision has been enabled.
  // These values are logged to UMA. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(FamilyLinkUserLogSegment)
  enum class Segment {
    // User is not a supervised child or parent in FamilyLink.
    kUnsupervised = 0,
    // Profile list contains only users that are required to be supervised
    // by FamilyLink due to child account policies (maps to Unicorn and
    // Griffin accounts).
    kSupervisionEnabledByFamilyLinkPolicy = 1,
    // Profile list contains only users that have chosen to be supervised by
    // FamilyLink (maps to Geller accounts).
    kSupervisionEnabledByFamilyLinkUser = 2,
    // Profile list contains at least one primary account that is supervised.
    kMixedProfile = 3,
    // Profile list contains only primary accounts identified as parents in
    // Family Link.
    kParent = 4,
    // Profile list contains profiles that had the supervision enabled locally
    // (e.g. on the device).
    kSupervisionEnabledLocally = 5,
    // Add future entries above this comment, in sync with
    // "FamilyLinkUserLogSegment" in src/tools/metrics/histograms/enums.xml.
    // Update kMaxValue to the last value.
    kMaxValue = kSupervisionEnabledLocally
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/families/enums.xml:FamilyLinkUserLogSegment)

  // Returns an immutable FamilyLinkUserLogRecord.
  static FamilyLinkUserLogRecord Create(
      signin::IdentityManager* identity_manager,
      const PrefService& pref_service,
      const HostContentSettingsMap& content_settings_map,
      SupervisedUserService* supervised_user_service);

  // Returns the supervision status of the primary account.
  std::optional<Segment> GetSupervisionStatusForPrimaryAccount() const;

  // Returns the web filter applied to the account if it is supervised,
  // otherwise returns nullopt.
  std::optional<WebFilterType> GetWebFilterTypeForPrimaryAccount() const;

  // Returns the state of the parent toggle for website permissions if the
  // primary account is supervised, otherwise returns nullopt.
  std::optional<ToggleState> GetPermissionsToggleStateForPrimaryAccount() const;

  // Returns the state of the parent toggle for extensions approvals if the
  // primary account is supervised, otherwise returns nullopt.
  std::optional<ToggleState> GetExtensionsToggleStateForPrimaryAccount() const;

 private:
  FamilyLinkUserLogRecord(std::optional<Segment> supervision_status,
                          std::optional<WebFilterType> web_filter_type,
                          std::optional<ToggleState> permissions_toggle_state,
                          std::optional<ToggleState> extensions_toggle_state);

  std::optional<Segment> supervision_status_;
  std::optional<WebFilterType> web_filter_type_;
  std::optional<ToggleState> permissions_toggle_state_;
  std::optional<ToggleState> extensions_toggle_state_;
};

// Declaration for gtest: defining in prod code is not required.
void PrintTo(FamilyLinkUserLogRecord::Segment segment, std::ostream* os);

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_FAMILY_LINK_USER_LOG_RECORD_H_
