// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_constants.h"

namespace password_manager {

const base::FilePath::CharType kAffiliationDatabaseFileName[] =
    FILE_PATH_LITERAL("Affiliation Database");
const base::FilePath::CharType kLoginDataForProfileFileName[] =
    FILE_PATH_LITERAL("Login Data");
const base::FilePath::CharType kLoginDataForAccountFileName[] =
    FILE_PATH_LITERAL("Login Data For Account");

const char kPasswordManagerAccountDashboardURL[] =
    "https://passwords.google.com";

const char kPasswordManagerHelpCenterSmartLock[] =
    "https://support.google.com/accounts?p=smart_lock_chrome";

}  // namespace password_manager
