// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_init_state.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

DEFINE_USER_DATA(BrowserInitState);

// static
BrowserInitState* BrowserInitState::From(BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

// static
const BrowserInitState* BrowserInitState::From(
    const BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

BrowserInitState::BrowserInitState(const Browser::CreateParams& params,
                                   ui::UnownedUserDataHost& host)
    : create_params_(params),
      omit_from_session_restore_(params.omit_from_session_restore),
      should_trigger_session_restore_(params.should_trigger_session_restore),
      override_bounds_(params.initial_bounds),
      initial_show_state_(params.initial_show_state),
      initial_workspace_(params.initial_workspace),
      initial_visible_on_all_workspaces_state_(
          params.initial_visible_on_all_workspaces_state),
      creation_source_(params.creation_source),
      initial_vertical_tab_strip_collapsed_(
          params.vertical_tab_strip_collapsed),
      initial_vertical_tab_strip_uncollapsed_width_(
          params.vertical_tab_strip_uncollapsed_width),
      initial_focused_tab_group_id_(params.focused_tab_group_id),
      scoped_unowned_user_data_(host, *this) {}

BrowserInitState::~BrowserInitState() = default;
