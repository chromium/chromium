// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_INIT_STATE_H_
#define CHROME_BROWSER_UI_BROWSER_INIT_STATE_H_

#include <optional>
#include <string>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/gfx/geometry/rect.h"

class BrowserWindowInterface;

namespace ui {
class UnownedUserDataHost;
}  // namespace ui

// Holds the creation and initial parameters of a browser window. These values
// are seeded from Browser::CreateParams when the window is created and are
// mostly read-only afterwards (a few may be adjusted during early window
// setup). This state is window-scoped and attached to the browser's
// UnownedUserDataHost, so it can be reached from any holder of a
// BrowserWindowInterface via From().
class BrowserInitState {
 public:
  using CreationSource = Browser::CreationSource;

  DECLARE_USER_DATA(BrowserInitState);

  BrowserInitState(const Browser::CreateParams& params,
                   ui::UnownedUserDataHost& host);
  BrowserInitState(const BrowserInitState&) = delete;
  BrowserInitState& operator=(const BrowserInitState&) = delete;
  ~BrowserInitState();

  static BrowserInitState* From(BrowserWindowInterface* browser);
  static const BrowserInitState* From(const BrowserWindowInterface* browser);

  const Browser::CreateParams& create_params() const { return create_params_; }
  const BrowserWindowCreateParams& browser_window_create_params() const {
    return browser_window_create_params_;
  }

  CreationSource creation_source() const { return creation_source_; }

  // Set indicator that this browser is being created via session restore.
  // This is used on the Mac (only) to determine animation style when the
  // browser window is shown.
  void set_is_session_restore(bool is_session_restore) {
    creation_source_ = CreationSource::kSessionRestore;
  }
  bool is_session_restore() const {
    return creation_source_ == CreationSource::kSessionRestore;
  }

  // Set overrides for the initial window bounds and show state.
  void set_override_bounds(const gfx::Rect& bounds) {
    override_bounds_ = bounds;
  }
  gfx::Rect override_bounds() const { return override_bounds_; }
  // Return true if the initial window bounds have been overridden.
  bool bounds_overridden() const { return !override_bounds_.IsEmpty(); }

  ui::mojom::WindowShowState initial_show_state() const {
    return initial_show_state_;
  }
  void set_initial_show_state(ui::mojom::WindowShowState initial_show_state) {
    initial_show_state_ = initial_show_state;
  }

  const std::string& initial_workspace() const { return initial_workspace_; }
  bool initial_visible_on_all_workspaces_state() const {
    return initial_visible_on_all_workspaces_state_;
  }

  std::optional<bool> is_vertical_tabs_initially_collapsed() const {
    return initial_vertical_tab_strip_collapsed_;
  }
  std::optional<int> get_vertical_tabs_initial_uncollapsed_width() const {
    return initial_vertical_tab_strip_uncollapsed_width_;
  }

  std::optional<tab_groups::TabGroupId> initial_focused_tab_group_id() const {
    return initial_focused_tab_group_id_;
  }

  bool omit_from_session_restore() const { return omit_from_session_restore_; }
  bool should_trigger_session_restore() const {
    return should_trigger_session_restore_;
  }

 private:
  // This Browser's create params.
  const Browser::CreateParams create_params_;
  const BrowserWindowCreateParams browser_window_create_params_;

  // Whether this Browser should be omitted from being saved/restored by session
  // restore.
  const bool omit_from_session_restore_;

  // If true, a new window opening should be treated like the start of a session
  // (with potential session restore, startup URLs, etc.). Otherwise, don't
  // restore the session.
  const bool should_trigger_session_restore_;

  // Override values for the bounds of the window and its show state.
  // These are supplied by callers that don't want to use the default values.
  // The default values are typically loaded from local state (last session),
  // obtained from the last window of the same type, or obtained from the
  // shell shortcut's startup info.
  gfx::Rect override_bounds_;
  ui::mojom::WindowShowState initial_show_state_;

  const std::string initial_workspace_;
  const bool initial_visible_on_all_workspaces_state_;

  CreationSource creation_source_;

  const std::optional<bool> initial_vertical_tab_strip_collapsed_;
  const std::optional<int> initial_vertical_tab_strip_uncollapsed_width_;
  const std::optional<tab_groups::TabGroupId> initial_focused_tab_group_id_;

  ui::ScopedUnownedUserData<BrowserInitState> scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_INIT_STATE_H_
