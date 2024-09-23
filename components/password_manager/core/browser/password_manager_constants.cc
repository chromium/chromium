// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_constants.h"

namespace password_manager {

const base::FilePath::CharType kLoginDataForProfileFileName[] =
    FILE_PATH_LITERAL("Login Data");
const base::FilePath::CharType kLoginDataForAccountFileName[] =
    FILE_PATH_LITERAL("Login Data For Account");
const base::FilePath::CharType kLoginDataJournalForProfileFileName[] =
    FILE_PATH_LITERAL("Login Data-journal");
const base::FilePath::CharType kLoginDataJournalForAccountFileName[] =
    FILE_PATH_LITERAL("Login Data For Account-journal");

const char kPasswordManagerAccountDashboardURL[] =
    "https://passwords.google.com";

const char kPasswordManagerHelpCenteriOSURL[] =
    "https://support.google.com/chrome/answer/95606?ios=1";

const char kPasswordManagerHelpCenterSmartLock[] =
    "https://support.google.com/accounts?p=smart_lock_chrome";

const char kManageMyPasswordsURL[] = "https://passwords.google.com/app";

const char kReferrerURL[] = "https://passwords.google/";

}  // namespace password_manager
