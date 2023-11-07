// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_TEST_UTIL_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_TEST_UTIL_H_

#include <vector>

#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace views {
class MenuItemView;
class View;
}  // namespace views

namespace ash {

enum class HoldingSpaceCommandId;
class HoldingSpaceModel;

// Performs a click on `view` with optional `flags`.
void Click(const views::View* view, int flags = ui::EF_NONE);

// Returns the suggestion items in `model`.
std::vector<std::pair<HoldingSpaceItem::Type, base::FilePath>>
GetSuggestionsInModel(const HoldingSpaceModel& model);

// Performs a press and release of the specified `key_code` with `flags`.
void PressAndReleaseKey(ui::KeyboardCode key_code, int flags = ui::EF_NONE);

// Performs a right click on `view` with the specified `flags`.
void RightClick(const views::View* view, int flags = ui::EF_NONE);

// Selects the menu item with the specified command ID. Returns the selected
// menu item if successful, `nullptr` otherwise.
views::MenuItemView* SelectMenuItemWithCommandId(
    HoldingSpaceCommandId command_id);

// Waits until `expected_suggestions` are the only suggestion items in `model`.
// The order among `expected_suggestions` is respected.
void WaitForSuggestionsInModel(
    HoldingSpaceModel* model,
    const std::vector<std::pair<HoldingSpaceItem::Type, base::FilePath>>&
        expected_suggestions);

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_TEST_UTIL_H_
