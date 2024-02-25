// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_STANDALONE_WINDOW_MIGRATION_NUDGE_UTIL_H_
#define CHROME_BROWSER_UI_ASH_SHELF_STANDALONE_WINDOW_MIGRATION_NUDGE_UTIL_H_

#include <string>

namespace views {
class View;
}

namespace ash {
// This function creates a specific instance of `AnchoredNudgeData`, used to
// notify users of affected web apps now opening in standalone windows by
// default.
void CreateAndShowNudge(views::View* anchor_view, std::u16string view_title);

// Callback called when 'Learn more' button on nudge is selected. The actions
// of this callback are to be finalized, and have not yet been implemented.
void OnLearnMoreButtonSelected();

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_SHELF_STANDALONE_WINDOW_MIGRATION_NUDGE_UTIL_H_
