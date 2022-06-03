// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_REAUTH_PURPOSE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_REAUTH_PURPOSE_H_

namespace password_manager {

// The reason why the user is asked to pass a reauthentication challenge.
enum class ReauthPurpose {
  VIEW_PASSWORD,  // A password value will be made visible on the UI
  COPY_PASSWORD,  // A password value will be copied into the clipboard
  EDIT_PASSWORD,  // A password value will be edited in the UI
  EXPORT,         // Password values will be written to the filesystem without
                  // protection
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_REAUTH_PURPOSE_H_
