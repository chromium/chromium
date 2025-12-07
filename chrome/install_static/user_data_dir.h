// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALL_STATIC_USER_DATA_DIR_H_
#define CHROME_INSTALL_STATIC_USER_DATA_DIR_H_

#include <string>

namespace install_static {

struct InstallConstants;

// Populates |result| with the user data dir, respecting various overrides in
// the manner of chrome_main_delegate.cc InitializeUserDataDir(). This includes
// overrides on the command line, overrides by registry policy, and fallback to
// the default User Data dir if the directory is invalid or unspecified.
//
// If the process command has --headless switch and user data directory is not
// provided by command line or registry policy overrides, a temporary unique
// user data directory will be created and made available to headless mode
// initialization code in headless::InitHeadlessMode(). This allows headless
// Chrome instances to run in parallel and with headful Chrome also running.
//
// If a directory was given by the user (either on the command line, or by
// registry policy), but it was invalid or unusable, then
// |invalid_supplied_directory| will be filled with the value that was unusable
// for reporting an error to the user.
//
// Other than in test situations, it is generally only appropriate to call this
// function once on startup and use the result for subsequent callers, otherwise
// there's a race with registry modification (which could cause a different
// derivation) so different subsystems would see different values). In normal
// usage, it should be called only once and cached. GetUserDataDirectory() does
// this, and should be preferred.
bool GetUserDataDirectoryImpl(const std::wstring& command_line,
                              const InstallConstants& mode,
                              std::wstring* result,
                              std::wstring* invalid_supplied_directory);

// Retrieves the user data directory, and any invalid directory specified on the
// command line, for reporting an error to the user. These values are cached on
// the first call. |invalid_user_data_directory| may be null if not required.
bool GetUserDataDirectory(std::wstring* user_data_directory,
                          std::wstring* invalid_user_data_directory);

// Returns true if GetUserDataDirectory[Impl]() returns a temporary user
// data directory created when running in headless mode with no explicit user
// data directory specification.
bool IsTemporaryUserDataDirectoryCreatedForHeadless();

}  // namespace install_static

#endif  // CHROME_INSTALL_STATIC_USER_DATA_DIR_H_
