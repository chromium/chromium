// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SHARED_TAB_GROUP_VERSION_UPGRADE_MODAL_H_
#define CHROME_BROWSER_UI_TABS_SHARED_TAB_GROUP_VERSION_UPGRADE_MODAL_H_

class Browser;

namespace tab_groups {

// Displays a modal dialog prompting the user to update Chrome when their client
// is too old to open a shared tab group.
void MaybeShowSharedTabGroupVersionOutOfDateModal(Browser* browser);

// Displays a toast notification after a successful Chrome update, indicating
// that the user can now see and use shared tab groups.
void MaybeShowSharedTabGroupVersionUpToDateToast(Browser* browser);

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_TABS_SHARED_TAB_GROUP_VERSION_UPGRADE_MODAL_H_
