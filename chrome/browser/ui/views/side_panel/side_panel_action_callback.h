// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ACTION_CALLBACK_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ACTION_CALLBACK_H_

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "ui/actions/actions.h"
#include "ui/base/class_property.h"

actions::ActionItem::InvokeActionCallback CreateToggleSidePanelActionCallback(
    SidePanelEntryKey key,
    Browser* browser);

extern const ui::ClassProperty<
    std::underlying_type_t<SidePanelOpenTrigger>>* const
    kSidePanelOpenTriggerKey;

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ACTION_CALLBACK_H_
