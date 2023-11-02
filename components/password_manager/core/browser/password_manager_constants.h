// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_CONSTANTS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_CONSTANTS_H_

#include "base/files/file_path.h"

namespace password_manager {

extern const base::FilePath::CharType kAffiliationDatabaseFileName[];
extern const base::FilePath::CharType kLoginDataForProfileFileName[];
extern const base::FilePath::CharType kLoginDataForAccountFileName[];

// URL to the password manager account dashboard.
extern const char kPasswordManagerAccountDashboardURL[];

// URL to the password manager help center.
extern const char kPasswordManagerHelpCenteriOSURL[];

// URL to the help center article about Smart Lock;
// TODO(crbug.com/862269): remove when "Smart Lock" is completely gone.
extern const char kPasswordManagerHelpCenterSmartLock[];

// URL which open native Password Manager UI.
extern const char kManageMyPasswordsURL[];

// URL from which native Password Manager UI can be opened.
extern const char kReferrerURL[];

// URL for a testing website from which native Password Manager UI can be
// opened.
// TODO(crbug.com/1329165): remove when the main website is launched.
extern const char kTestingReferrerURL[];

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_CONSTANTS_H_
