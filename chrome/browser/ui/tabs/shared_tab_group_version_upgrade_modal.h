// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SHARED_TAB_GROUP_VERSION_UPGRADE_MODAL_H_
#define CHROME_BROWSER_UI_TABS_SHARED_TAB_GROUP_VERSION_UPGRADE_MODAL_H_

class Browser;

namespace tab_groups {

// Checks if the shared tab group version upgrade modal should be shown and
// displays it if necessary.
void MaybeShowSharedTabGroupVersionUpgradeModal(Browser* browser);

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_TABS_SHARED_TAB_GROUP_VERSION_UPGRADE_MODAL_H_
