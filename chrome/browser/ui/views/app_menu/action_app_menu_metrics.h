// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_METRICS_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_METRICS_H_

#include <string_view>

#include "base/timer/elapsed_timer.h"
#include "ui/actions/action_id.h"

namespace actions {
class BaseAction;
}  // namespace actions

// Records UMA histograms and user actions for ActionAppMenu.
class ActionAppMenuMetrics {
 public:
  ActionAppMenuMetrics();
  ActionAppMenuMetrics(const ActionAppMenuMetrics&) = delete;
  ActionAppMenuMetrics& operator=(const ActionAppMenuMetrics&) = delete;
  ~ActionAppMenuMetrics();

  // Resets the menu open timer and records metrics for opening the app menu.
  void OnMenuOpened();

  // Records metrics when a submenu is about to be shown.
  void OnWillShowSubMenu(int command_id);

  // Records metrics for a menu item represented by `base_action`, handling both
  // static ActionIds and dynamically generated submenu items (e.g. bookmarks,
  // recent tabs, profiles).
  void LogMenuAction(actions::BaseAction* base_action);

 private:
  // Records metrics for a menu item with a known ActionId.
  void LogMenuActionWithId(actions::ActionId action_id);

  // Records `WrenchMenu.MenuAction` and, if this is the first action recorded
  // since the menu was opened, `WrenchMenu.TimeToAction` and
  // `WrenchMenu.TimeToAction.<time_to_action_suffix>`.
  void RecordAction(int action_id, std::string_view time_to_action_suffix);

  // Records `WrenchMenu.TimeToAction` if no action has been recorded since the
  // menu was opened.
  void RecordTimeToAction();

  // Time the menu has been open. Used to record WrenchMenu.TimeToAction*
  // histograms when an item is selected.
  base::ElapsedTimer menu_opened_timer_;

  // Whether a UMA menu action has been recorded since the menu was opened.
  // Prevents recording TimeToAction or repeated zoom actions multiple times
  // when in-place menu controls (like Zoom +/-) keep the menu open.
  bool uma_action_recorded_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_METRICS_H_
