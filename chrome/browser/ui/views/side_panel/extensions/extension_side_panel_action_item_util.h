// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_EXTENSIONS_EXTENSION_SIDE_PANEL_ACTION_ITEM_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_EXTENSIONS_EXTENSION_SIDE_PANEL_ACTION_ITEM_UTIL_H_

#include "extensions/common/extension_id.h"

class BrowserWindowInterface;

namespace extensions {

class Extension;

// An extension's side panel action item lives in a window's browser action
// tree and is shared by the extension's global SidePanelEntry and the
// contextual SidePanelEntry of every tab in that window. These helpers
// reference-count the entries that use it: the action item is created with the
// first referencing entry and removed once the last one is deregistered.
namespace side_panel_action_item_util {

// Records that one more SidePanelEntry references `extension`'s side panel
// action item in `browser`, creating the action item if no entry referenced it
// yet.
void AcquireActionItem(BrowserWindowInterface* browser,
                       const Extension& extension);

// Records that one fewer SidePanelEntry references the side panel action item
// for `extension_id` in `browser`, removing the action item once the last
// reference is dropped.
void ReleaseActionItem(BrowserWindowInterface* browser,
                       const ExtensionId& extension_id);

}  // namespace side_panel_action_item_util

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_EXTENSIONS_EXTENSION_SIDE_PANEL_ACTION_ITEM_UTIL_H_
