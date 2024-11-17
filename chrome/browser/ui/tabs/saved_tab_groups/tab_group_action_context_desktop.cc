// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_action_context_desktop.h"

#include "chrome/browser/ui/browser.h"
#include "components/saved_tab_groups/public/types.h"

namespace tab_groups {

TabGroupActionContextDesktop::TabGroupActionContextDesktop(
    Browser* browser,
    OpeningSource opening_source)
    : browser(browser), opening_source(opening_source) {}
}  // namespace tab_groups
