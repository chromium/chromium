// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_SUBRESOURCE_FILTER_CONSTANTS_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_SUBRESOURCE_FILTER_CONSTANTS_H_

#include "base/files/file_path.h"

namespace subresource_filter {

// The name of the top-level directory under the user data directory that
// contains all files and subdirectories related to the subresource filter.
extern const base::FilePath::CharType kTopLevelDirectoryName[];

// Paths under |kTopLevelDirectoryName|
// ------------------------------------

// The name of the subdirectory under the top-level directory that stores
// versions of indexed rulesets. Files that belong to an IndexedRulesetVersion
// are stored under /format_version/content_version/.
extern const base::FilePath::CharType kIndexedRulesetBaseDirectoryName[];

// The name of the subdirectory under the top-level directory that stores
// versions of unindexed rulesets downloaded through the component updater.
extern const base::FilePath::CharType kUnindexedRulesetBaseDirectoryName[];

// Paths under IndexedRulesetVersion::GetSubdirectoryPathForVersion
// ----------------------------------------------------------------

// The name of the file that actually stores the ruleset contents.
extern const base::FilePath::CharType kRulesetDataFileName[];

// The name of the applicable license file, if any, stored next to the ruleset.
extern const base::FilePath::CharType kLicenseFileName[];

// The name of the sentinel file that is temporarily stored to indicate that the
// ruleset is being indexed.
extern const base::FilePath::CharType kSentinelFileName[];

// Paths under kUnindexedRulesetBaseDirectoryName
// ----------------------------------------------

// The name of the license file associated with the unindex ruleset.
extern const base::FilePath::CharType kUnindexedRulesetLicenseFileName[];

// The name of the file that stores the unindexed filtering rules.
extern const base::FilePath::CharType kUnindexedRulesetDataFileName[];

// Console message to be displayed on activation.
constexpr char kActivationConsoleMessage[] =
    "Chrome is blocking ads on this site because this site tends to show ads "
    "that interrupt, distract, mislead, or prevent user control. You should "
    "fix the issues as soon as possible and submit your site for another "
    "review. Learn more at "
    "https://www.chromestatus.com/feature/5738264052891648";

constexpr char kActivationWarningConsoleMessage[] =
    "Chrome might start blocking ads on this site in the future because this "
    "site tends to show ads that interrupt, distract, mislead, or prevent user "
    "control. You should fix the issues as soon as possible and submit your "
    "site for another review. Learn more more at "
    "https://www.chromestatus.com/feature/5738264052891648";

// Console message to be displayed on disallowing subframe.
constexpr char kDisallowSubframeConsoleMessageFormat[] =
    "Chrome blocked resource %s on this site because this site tends to show "
    "ads that interrupt, distract, mislead, or prevent user control. Learn "
    "more at https://www.chromestatus.com/feature/5738264052891648";

constexpr char kLearnMoreLink[] =
    "https://support.google.com/chrome/?p=blocked_ads";

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_SUBRESOURCE_FILTER_CONSTANTS_H_
