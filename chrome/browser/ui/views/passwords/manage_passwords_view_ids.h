// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_VIEW_IDS_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_VIEW_IDS_H_

namespace password_manager {

// Defines an enumeration of IDs that can uniquely identify a view within the
// scope of password management bubble. Used to validate views in browser tests.
enum class ManagePasswordsViewIDs {
  kManagePasswordsButton = 1,
  kCopyUsernameButton,
  kCopyPasswordButton,
  kEditUsernameButton,
  kEditNoteButton,
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_VIEW_IDS_H_
