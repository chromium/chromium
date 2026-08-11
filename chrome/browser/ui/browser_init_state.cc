// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_init_state.h"

#include "build/build_config.h"
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

BrowserInitState::BrowserInitState(BrowserWindowCreateParams params,
                                   ui::UnownedUserDataHost& host)
    : browser_window_create_params_(std::move(params)),
      omit_from_session_restore_(
          browser_window_create_params_.omit_from_session_restore),
      should_trigger_session_restore_(
          browser_window_create_params_.should_trigger_session_restore),
      override_bounds_(browser_window_create_params_.initial_bounds),
      initial_show_state_(browser_window_create_params_.initial_show_state),
      initial_workspace_(browser_window_create_params_.initial_workspace),
      initial_visible_on_all_workspaces_state_(
          browser_window_create_params_
              .initial_visible_on_all_workspaces_state),
      creation_source_(browser_window_create_params_.creation_source),
      initial_vertical_tab_strip_collapsed_(
          browser_window_create_params_.vertical_tab_strip_collapsed),
      initial_vertical_tab_strip_uncollapsed_width_(
          browser_window_create_params_.vertical_tab_strip_uncollapsed_width),
      initial_focused_tab_group_id_(
          browser_window_create_params_.focused_tab_group_id),
      scoped_unowned_user_data_(host, *this) {}

BrowserInitState::~BrowserInitState() = default;
