// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORDS_DIRECTORY_UTIL_IOS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORDS_DIRECTORY_UTIL_IOS_H_

namespace base {
class FilePath;
}

namespace password_manager {

// Fills |directory_path| with the FilePath to the passwords temporary
// directory used for exporting passwords on iOS.
// Returns true if this is successful.
bool GetPasswordsDirectory(base::FilePath* directory_path);

// Asynchronously deletes the temporary passwords directory.
void DeletePasswordsDirectory();

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORDS_DIRECTORY_UTIL_IOS_H_
