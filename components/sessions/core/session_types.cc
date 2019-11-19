// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/session_types.h"

#include <stddef.h>

#include "components/sessions/core/session_command.h"

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

// SessionTab -----------------------------------------------------------------

SessionTabGroup::SessionTabGroup(base::Token group_id) : group_id(group_id) {}

SessionTabGroup::~SessionTabGroup() {}

// SessionWindow ---------------------------------------------------------------

SessionWindow::SessionWindow()
    : window_id(SessionID::NewUnique()),
      selected_tab_index(-1),
      type(TYPE_NORMAL),
      is_constrained(true),
      show_state(ui::SHOW_STATE_DEFAULT) {}

SessionWindow::~SessionWindow() {}

}  // namespace sessions
