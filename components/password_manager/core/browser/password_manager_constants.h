// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_CONSTANTS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_CONSTANTS_H_

#include "base/files/file_path.h"
#include "base/time/time.h"

namespace password_manager {

extern const base::FilePath::CharType kLoginDataForProfileFileName[];
extern const base::FilePath::CharType kLoginDataForAccountFileName[];
extern const base::FilePath::CharType kLoginDataJournalForProfileFileName[];
extern const base::FilePath::CharType kLoginDataJournalForAccountFileName[];

// URL to the password manager account dashboard.
extern const char kPasswordManagerAccountDashboardURL[];

// URL to the password manager help center.
extern const char kPasswordManagerHelpCenteriOSURL[];

// URL to the help center article about Smart Lock;
// TODO(crbug.com/40584353): remove when "Smart Lock" is completely gone.
extern const char kPasswordManagerHelpCenterSmartLock[];

// URL which open native Password Manager UI.
extern const char kManageMyPasswordsURL[];

// URL from which native Password Manager UI can be opened.
extern const char kReferrerURL[];

// The size of LRU cache used to store single username candidates.
inline constexpr int kMaxSingleUsernameFieldsToStore = 10;

// After `kSingleUsernameTimeToLive` single username candidate is no longer
// considered as username.
inline constexpr base::TimeDelta kSingleUsernameTimeToLive = base::Minutes(5);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_CONSTANTS_H_
