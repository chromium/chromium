// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TEST_INTEGRATION_TESTS_MAC_H_
#define CHROME_UPDATER_TEST_INTEGRATION_TESTS_MAC_H_

#include <string>

#include "chrome/updater/updater_scope.h"

namespace base {
class FilePath;
}

namespace updater::test {

// Create a CRURegistration and expect its syncFindBestKSAdmin to return nil.
void ExpectCRURegistrationCannotFindKSAdmin();

// Create a CRURegistration and expect its syncFindBestKSAdmin to return the
// path to the ksadmin installed as part of the Keystone shim at the provided
// updater scope.
void ExpectCRURegistrationFindsKSAdmin(UpdaterScope scope);

// Create a CRURegistration and expect it to encounter some error when
// attempting to fetch the tag for the specified app ID. CRURegistration is
// always used in-process as the current user; it never elevates.
void ExpectCRURegistrationCannotFetchTag(const std::string& app_id,
                                         const base::FilePath& xc_path);

// Create a CRURegistration and expect it to successfully fetch the specified
// tag for the specified app ID. CRURegistration is always used in-process as
// the current user; it never elevates.
void ExpectCRURegistrationFetchesTag(const std::string& app_id,
                                     const base::FilePath& xc_path,
                                     const std::string& tag);

// Create a CRURegistration and expect it to successfully register the specified
// app for updates. CRURegistration is always used in-process as the current
// user; it never elevates.
void ExpectCRURegistrationRegisters(const std::string& app_id,
                                    const base::FilePath& xc_path,
                                    const std::string& version_str);

// Create a CRURegistration and expect it to fail to register the specified
// app for updates. CRURegistration is always used in-process as the current
// user; it never elevates.
void ExpectCRURegistrationCannotRegister(const std::string& app_id,
                                         const base::FilePath& xc_path,
                                         const std::string& version_str);

}  // namespace updater::test

#endif  // CHROME_UPDATER_TEST_INTEGRATION_TESTS_MAC_H_
