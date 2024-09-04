// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_UTILS_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_UTILS_H_

#include <string>
#include <vector>

#include "base/memory/raw_ref.h"
#include "components/safe_search_api/url_checker.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/supervised_user/core/browser/family_link_user_log_record.h"
#include "components/supervised_user/core/browser/proto/families_common.pb.h"

class GURL;
class PrefService;

namespace supervised_user {

// Reason for applying the website filtering parental control.
enum class FilteringBehaviorReason {
  DEFAULT = 0,
  ASYNC_CHECKER = 1,
  MANUAL = 2,
};

// Details degarding how a particular filtering classification was arrived at.
struct FilteringBehaviorDetails {
  FilteringBehaviorReason reason;

  // The following field only applies if `reason` is `ASYNC_CHECKER`.
  safe_search_api::ClassificationDetails classification_details;
};

// A Java counterpart will be generated for this enum.
// Values are stored in prefs under kDefaultSupervisedUserFilteringBehavior.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.superviseduser
enum class FilteringBehavior : int {
  kAllow = 0,
  // Deprecated, kWarn = 1.
  kBlock = 2,
  kInvalid = 3,
};

// This enum describes the state of the interstitial banner that is shown for
// when previous supervised users of desktop see the interstitial for the first
// time after desktop controls are enabled.
enum class FirstTimeInterstitialBannerState : int {
  // Supervised users should see banner the next time the interstitial is
  // triggered.
  kNeedToShow = 0,

  // Banner has been shown to supervised user if needed.
  kSetupComplete = 1,

  // Banner state has not been set.
  kUnknown = 2,
};

// Whether the migration of existing extensions to parent-approved needs to be
// executed, when the feature
// `kEnableSupervisedUserSkipParentApprovalToInstallExtensions` becomes enabled.
enum class LocallyParentApprovedExtensionsMigrationState : int {
  kNeedToRun = 0,
  kComplete = 1,
};

// Converts FamilyRole enum to string format.
std::string FamilyRoleToString(kidsmanagement::FamilyRole role);

// Strips user-specific tokens in a URL to generalize it.
GURL NormalizeUrl(const GURL& url);

// Check if web filtering prefs are set to default values.
bool AreWebFilterPrefsDefault(const PrefService& pref_service);

// Given a list of records that map to the supervision state of primary
// accounts on the user's device, emits metrics that reflect the FamilyLink
// settings of the user.
// Returns true if one or more histograms were emitted.
bool EmitLogRecordHistograms(
    const std::vector<FamilyLinkUserLogRecord>& records);

// Url formatter helper.
// Decisions on how to format the url depend on the filtering reason,
// the manual parental url block-list.
class UrlFormatter {
 public:
  UrlFormatter(const SupervisedUserURLFilter& supervised_user_url_filter,
               FilteringBehaviorReason filtering_behavior_reason);
  ~UrlFormatter();
  GURL FormatUrl(const GURL& url) const;

 private:
  const raw_ref<const SupervisedUserURLFilter> supervised_user_url_filter_;
  const FilteringBehaviorReason filtering_behavior_reason_;
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_UTILS_H_
