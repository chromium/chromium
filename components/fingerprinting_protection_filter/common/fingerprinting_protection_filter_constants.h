// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_COMMON_FINGERPRINTING_PROTECTION_FILTER_CONSTANTS_H_
#define COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_COMMON_FINGERPRINTING_PROTECTION_FILTER_CONSTANTS_H_

#include "base/files/file_path.h"
#include "components/subresource_filter/core/common/ruleset_config.h"

namespace fingerprinting_protection_filter {

// The config used to identify the Fingerprinting Protection ruleset for the
// RulesetService. Encompasses a ruleset tag and top level directory name where
// the ruleset should be stored.
extern const subresource_filter::RulesetConfig
    kFingerprintingProtectionRulesetConfig;

// The name of the file that stores the unindexed filtering rules for
// Fingerprinting Protection.
extern const base::FilePath::CharType kUnindexedRulesetDataFileName[];

const char kPageActivationThrottleNameForLogging[] =
    "FingerprintingProtectionPageActivationThrottle";

// Histogram names
const char ActivationDecisionHistogramName[] =
    "FingerprintingProtection.PageLoad.ActivationDecision";

const char ActivationLevelHistogramName[] =
    "FingerprintingProtection.PageLoad.ActivationLevel";

const char MainFrameLoadRulesetIsAvailableAnyActivationLevelHistogramName[] =
    "FingerprintingProtection.MainFrameLoad."
    "RulesetIsAvailableAnyActivationLevel";

const char DocumentLoadRulesetIsAvailableHistogramName[] =
    "FingerprintingProtection.DocumentLoad.RulesetIsAvailable";

const char RefreshCountHistogramName[] =
    "FingerprintingProtection.WebContentsObserver.RefreshCount";

const char HasRefreshCountExceptionHistogramName[] =
    "FingerprintingProtection.PageLoad.RefreshCount.SiteHasBreakageException";

const char AddRefreshCountExceptionHistogramName[] =
    "FingerprintingProtection.WebContentsObserver.RefreshCount."
    "AddBreakageException";

const char HasRefreshCountExceptionWallDurationHistogramName[] =
    "FingerprintingProtection.PageLoad.RefreshCount."
    "SiteHasBreakageExceptionWallDuration";

const char AddRefreshCountExceptionWallDurationHistogramName[] =
    "FingerprintingProtection.WebContentsObserver.RefreshCount."
    "AddBreakageExceptionWallDuration";

// Console messages
// ----------------

// Console message to be displayed the first time anything is blocked on a page.
inline constexpr char kDisallowFirstResourceConsoleMessage[] =
    "Fingerprinting protection blocked one or more resources on the current "
    "page. In case of site breakage, you can disable the feature via the "
    "chrome://flags#enable-fingerprinting-protection-blocklist flag or report "
    "bugs at https://issues.chromium.org/"
    "issues?q=status:open%20componentid:1456351&s=created_time:desc.";

// Console message to be displayed on disallowing a subframe. Blocking
// subresources automatically generates messages with an error code, which is
// not the case for navigations.
inline constexpr char kDisallowChildFrameConsoleMessageFormat[] =
    "Fingerprinting protection blocking navigation: %s";

}  // namespace fingerprinting_protection_filter

#endif  // COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_COMMON_FINGERPRINTING_PROTECTION_FILTER_CONSTANTS_H_
