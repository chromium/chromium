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

// Creates a CRURegistration and expects its syncFindBestKSAdmin to return nil.
void ExpectCRURegistrationCannotFindKSAdmin();

// Creates a CRURegistration and expects its syncFindBestKSAdmin to return the
// path to the ksadmin installed as part of the Keystone shim at the provided
// updater scope.
void ExpectCRURegistrationFindsKSAdmin(UpdaterScope scope);

// Creates a CRURegistration and expects it to encounter some error when
// attempting to fetch the tag for the specified app ID. CRURegistration is
// always used in-process as the current user; it never elevates.
void ExpectCRURegistrationCannotFetchTag(const std::string& app_id,
                                         const base::FilePath& xc_path);

// Creates a CRURegistration and expects it to successfully fetch the specified
// tag for the specified app ID. CRURegistration is always used in-process as
// the current user; it never elevates.
void ExpectCRURegistrationFetchesTag(const std::string& app_id,
                                     const base::FilePath& xc_path,
                                     const std::string& tag);

// Creates a CRURegistration and expects it to successfully register the
// specified app for updates. CRURegistration is always used in-process as the
// current user; it never elevates.
void ExpectCRURegistrationRegisters(const std::string& app_id,
                                    const base::FilePath& xc_path,
                                    const std::string& version_str);

// Creates a CRURegistration and expects it to fail to register the specified
// app for updates. CRURegistration is always used in-process as the current
// user; it never elevates.
void ExpectCRURegistrationCannotRegister(const std::string& app_id,
                                         const base::FilePath& xc_path,
                                         const std::string& version_str);

// Creates a CRURegistration and expects it to successfully mark the specified
// app active. CRURegistration is always used in-process as the current user;
// it never elevates.
void ExpectCRURegistrationMarksActive(const std::string& app_id);

// Launches the executable `registration_test_app_bundle` to install the updater
// as an unprivileged user via CRURegistration, pulling the installer out from
// inside the application bundle.
void ExpectRegistrationTestAppUserUpdaterInstallSuccess();

// Launches the executable `registration_test_app_bundle` to register itself
// with an (already-installed) user updater via CRURegistration.
void ExpectRegistrationTestAppRegisterSuccess();

// Launches the executable `registration_test_app_bundle` to install the updater
// as an unprivileged user via CRURegistration, pulling the installer out from
// inside the application bundle, and then register itself for updates.
void ExpectRegistrationTestAppInstallAndRegisterSuccess();

}  // namespace updater::test

#endif  // CHROME_UPDATER_TEST_INTEGRATION_TESTS_MAC_H_
