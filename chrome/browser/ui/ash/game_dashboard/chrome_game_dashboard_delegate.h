// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_GAME_DASHBOARD_CHROME_GAME_DASHBOARD_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_GAME_DASHBOARD_CHROME_GAME_DASHBOARD_DELEGATE_H_

#include "ash/components/arc/mojom/app.mojom-shared.h"
#include "ash/game_dashboard/game_dashboard_delegate.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/window.h"

class ChromeGameDashboardDelegate : public ash::GameDashboardDelegate {
 public:
  ChromeGameDashboardDelegate();
  ChromeGameDashboardDelegate(const ChromeGameDashboardDelegate&) = delete;
  ChromeGameDashboardDelegate& operator=(const ChromeGameDashboardDelegate&) =
      delete;
  ~ChromeGameDashboardDelegate() override;

  // ash::GameDashboardDelegate:
  void GetIsGame(const std::string& app_id, IsGameCallback callback) override;
  std::string GetArcAppName(const std::string& app_id) const override;
  void RecordGameWindowOpenedEvent(aura::Window* window) override;

 private:
  // Callback when `IsGame` queries ARC to get the app category.
  void OnReceiveAppCategory(IsGameCallback callback,
                            arc::mojom::AppCategory category);

  base::WeakPtrFactory<ChromeGameDashboardDelegate> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_GAME_DASHBOARD_CHROME_GAME_DASHBOARD_DELEGATE_H_
