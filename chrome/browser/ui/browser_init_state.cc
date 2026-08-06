// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_init_state.h"

#include "build/build_config.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

DEFINE_USER_DATA(BrowserInitState);

namespace {

BrowserWindowCreateParams MakeBrowserWindowCreateParams(
    const Browser::CreateParams& create_params) {
  BrowserWindowCreateParams window_create_params(
      create_params.type, *create_params.profile, create_params.user_gesture);
  window_create_params.initial_bounds = create_params.initial_bounds;
  window_create_params.is_trusted_source = create_params.trusted_source;
  window_create_params.app_name = create_params.app_name;
  window_create_params.initial_show_state = create_params.initial_show_state;
#if !BUILDFLAG(IS_ANDROID)
  window_create_params.omit_from_session_restore =
      create_params.omit_from_session_restore;
  window_create_params.should_trigger_session_restore =
      create_params.should_trigger_session_restore;
  window_create_params.initial_origin_specified =
      static_cast<BrowserWindowCreateParams::ValueSpecified>(
          create_params.initial_origin_specified);
  window_create_params.initial_workspace = create_params.initial_workspace;
  window_create_params.initial_visible_on_all_workspaces_state =
      create_params.initial_visible_on_all_workspaces_state;
  window_create_params.creation_source =
      static_cast<BrowserWindowCreateParams::CreationSource>(
          create_params.creation_source);
  window_create_params.in_tab_dragging = create_params.in_tab_dragging;
  window_create_params.window = create_params.window;
  window_create_params.user_title = create_params.user_title;
  window_create_params.can_resize = create_params.can_resize;
  window_create_params.can_maximize = create_params.can_maximize;
  window_create_params.can_fullscreen = create_params.can_fullscreen;
  window_create_params.pip_options = create_params.pip_options;
  window_create_params.vertical_tab_strip_collapsed =
      create_params.vertical_tab_strip_collapsed;
  window_create_params.vertical_tab_strip_uncollapsed_width =
      create_params.vertical_tab_strip_uncollapsed_width;
  window_create_params.focused_tab_group_id =
      create_params.focused_tab_group_id;
#if BUILDFLAG(IS_CHROMEOS)
  window_create_params.display_id = create_params.display_id;
#endif
#if BUILDFLAG(IS_LINUX)
  window_create_params.startup_id = create_params.startup_id;
#endif
#if BUILDFLAG(IS_OZONE)
  window_create_params.restore_id = create_params.restore_id;
#endif
#endif  // !BUILDFLAG(IS_ANDROID)
  return window_create_params;
}

}  // namespace

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
      browser_window_create_params_(
          MakeBrowserWindowCreateParams(create_params_)),
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
