// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_LOG_RECORD_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_LOG_RECORD_H_

#include <optional>
#include <ostream>
#include <vector>

#include "components/supervised_user/core/common/supervised_user_constants.h"

class HostContentSettingsMap;
class PrefService;

namespace signin {
class IdentityManager;
}

namespace supervised_user {
// TODO(crbug.com/465437113): Resolve dependency cycle.
class SupervisedUserUrlFilteringService;
class DeviceParentalControls;

// Stores information required to log UMA record histograms for supervised
// profiles (Family Link accounts or locally supervised devices).
class SupervisedUserLogRecord {
 public:
  // These enum values represent the user's supervision type and how the
  // supervision has been enabled.
  // These values are logged to UMA. Entries should not be renumbered and
  // numeric values should never be reused. NOTE: enum entries are not in order.
  // LINT.IfChange(SupervisedUserLogSegment)
  enum class Segment {
    // User is not a supervised child or parent in FamilyLink.
    kUnsupervised = 0,

    // SUPERVISED PROFILE TYPES
    // Profile list contains only users that are required to be supervised
    // by FamilyLink due to child account policies (maps to Unicorn and
    // Griffin accounts).
    kSupervisionEnabledByFamilyLinkPolicy = 1,
    // Profile list contains only users that have chosen to be supervised by
    // FamilyLink (maps to Geller accounts).
    kSupervisionEnabledByFamilyLinkUser = 2,
    // Profile list contains at least one primary account that is supervised
    kMixedProfile = 3,
    // Profile list contains only primary accounts identified as parents in
    // Family Link.
    kParent = 4,
    // Profile list contains profiles that had the supervision enabled locally
    // (e.g. on the device).
    kSupervisionEnabledLocally = 5,

    // Update kMaxValue to the last value.
    kMaxValue = kSupervisionEnabledLocally
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/families/enums.xml:SupervisedUserLogSegment)

  // Returns an immutable SupervisedUserLogRecord.
  static SupervisedUserLogRecord Create(
      signin::IdentityManager* identity_manager,
      const PrefService& pref_service,
      const HostContentSettingsMap& content_settings_map,
      SupervisedUserUrlFilteringService* url_filtering_service,
      const DeviceParentalControls& device_parental_controls);

  // Given a list of records that map to the supervision state of primary
  // accounts on the user's device, emits metrics that reflect the supervision
  // status of the user. On top of that, device-based supervision might emit an
  // extra record.
  // Returns true if one or more histograms were emitted.
  static bool EmitHistograms(
      const std::vector<SupervisedUserLogRecord>& records,
      const DeviceParentalControls& device_parental_controls);

  // Returns the supervision status of the primary account.
  std::optional<Segment> GetSupervisionStatusForPrimaryAccount() const;

  // Returns the web filter applied to the account if it is supervised,
  // otherwise the return value is empty.
  std::optional<WebFilterType> GetWebFilterTypeForPrimaryAccount() const;

  // Returns the state of the parent toggle for website permissions if the
  // primary account is supervised, otherwise the return value is empty.
  std::optional<ToggleState> GetPermissionsToggleStateForPrimaryAccount() const;

  // Returns the state of the parent toggle for extensions approvals if the
  // primary account is supervised, otherwise the return value is empty.
  std::optional<ToggleState> GetExtensionsToggleStateForPrimaryAccount() const;

 private:
  SupervisedUserLogRecord(std::optional<Segment> supervision_status,
                          std::optional<WebFilterType> web_filter_type,
                          std::optional<ToggleState> permissions_toggle_state,
                          std::optional<ToggleState> extensions_toggle_state);

  std::optional<Segment> supervision_status_;
  std::optional<WebFilterType> web_filter_type_;
  std::optional<ToggleState> permissions_toggle_state_;
  std::optional<ToggleState> extensions_toggle_state_;
};

// Declaration for gtest: defining in prod code is not required.
void PrintTo(SupervisedUserLogRecord::Segment segment, std::ostream* os);

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_LOG_RECORD_H_
