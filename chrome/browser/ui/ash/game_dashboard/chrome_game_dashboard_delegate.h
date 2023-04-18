// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_GAME_DASHBOARD_CHROME_GAME_DASHBOARD_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_GAME_DASHBOARD_CHROME_GAME_DASHBOARD_DELEGATE_H_

#include "ash/game_dashboard/game_dashboard_delegate.h"

class ChromeGameDashboardDelegate : public ash::GameDashboardDelegate {
 public:
  ChromeGameDashboardDelegate() = default;
  ChromeGameDashboardDelegate(const ChromeGameDashboardDelegate&) = delete;
  ChromeGameDashboardDelegate& operator=(const ChromeGameDashboardDelegate&) =
      delete;
  ~ChromeGameDashboardDelegate() override = default;

  // ash::GameDashboardDelegate:
  bool IsGame(const std::string& app_id) const override;
};

#endif  // CHROME_BROWSER_UI_ASH_GAME_DASHBOARD_CHROME_GAME_DASHBOARD_DELEGATE_H_
