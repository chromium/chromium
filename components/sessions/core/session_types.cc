// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/session_types.h"

#include <stddef.h>

#include "components/sessions/core/session_command.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/base/mojom/window_show_state.mojom.h"

namespace sessions {

// SessionTab -----------------------------------------------------------------

SessionTab::SessionTab()
    : window_id(SessionID::NewUnique()),
      tab_id(SessionID::NewUnique()),
      tab_visual_index(-1),
      current_navigation_index(-1),
      pinned(false) {}

SessionTab::~SessionTab() {
}

// SessionTabGroup -------------------------------------------------------------

SessionTabGroup::SessionTabGroup(const tab_groups::TabGroupId& id) : id(id) {}

SessionTabGroup::~SessionTabGroup() = default;

// SessionWindow ---------------------------------------------------------------

SessionWindow::SessionWindow()
    : window_id(SessionID::NewUnique()),
      visible_on_all_workspaces(false),
      selected_tab_index(-1),
      type(TYPE_NORMAL),
      is_constrained(true),
      show_state(ui::mojom::WindowShowState::kDefault) {}

SessionWindow::~SessionWindow() = default;

}  // namespace sessions
