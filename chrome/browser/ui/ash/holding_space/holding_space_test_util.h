// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_TEST_UTIL_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_TEST_UTIL_H_

#include <vector>

#include "ash/public/cpp/holding_space/holding_space_item.h"

namespace ash {

class HoldingSpaceModel;

// Returns the suggestion items in `model`.
std::vector<std::pair<HoldingSpaceItem::Type, base::FilePath>>
GetSuggestionsInModel(const HoldingSpaceModel& model);

// Waits until `expected_suggestions` are the only suggestion items in `model`.
// The order among `expected_suggestions` is respected.
void WaitForSuggestionsInModel(
    HoldingSpaceModel* model,
    const std::vector<std::pair<HoldingSpaceItem::Type, base::FilePath>>&
        expected_suggestions);

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_TEST_UTIL_H_
