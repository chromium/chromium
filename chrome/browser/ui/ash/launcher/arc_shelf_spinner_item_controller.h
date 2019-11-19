// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_SHELF_SPINNER_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_SHELF_SPINNER_ITEM_CONTROLLER_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/shelf_spinner_item_controller.h"

// ArcShelfSpinnerItemController displays the icon of the ARC app that
// cannot be launched immediately (due to ARC not being ready) on Chrome OS'
// shelf, with an overlaid spinner to provide visual feedback.
class ArcShelfSpinnerItemController : public ShelfSpinnerItemController,
                                      public ArcAppListPrefs::Observer,
                                      public arc::ArcSessionManager::Observer {
 public:
  ArcShelfSpinnerItemController(const std::string& arc_app_id,
                                int event_flags,
                                arc::UserInteractionType user_interaction_type,
                                int64_t display_id);

  ~ArcShelfSpinnerItemController() override;

  // ShelfSpinnerItemController:
  void SetHost(const base::WeakPtr<ShelfSpinnerController>& host) override;

  // ArcAppListPrefs::Observer:
  void OnAppStatesChanged(const std::string& app_id,
                          const ArcAppListPrefs::AppInfo& app_info) override;
  void OnAppRemoved(const std::string& removed_app_id) override;

  // arc::ArcSessionManager::Observer:
  void OnArcPlayStoreEnabledChanged(bool enabled) override;

 private:
  // The flags of the event that caused the ARC app to be activated. These will
  // be propagated to the launch event once the app is actually launched.
  const int event_flags_;

  // Stores how this action was initiated.
  const arc::UserInteractionType user_interaction_type_;

  const int64_t display_id_;

  // Unowned
  Profile* observed_profile_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ArcShelfSpinnerItemController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_ARC_SHELF_SPINNER_ITEM_CONTROLLER_H_
