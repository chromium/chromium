// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_ARC_SHELF_SPINNER_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_ARC_SHELF_SPINNER_ITEM_CONTROLLER_H_

#include <stdint.h>

#include <string>

#include "ash/components/arc/mojom/app.mojom.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/app_list/arc/intent.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "chrome/browser/ui/ash/shelf/shelf_spinner_item_controller.h"

// ArcShelfSpinnerItemController displays the icon of the ARC app that
// cannot be launched immediately (due to ARC not being ready) on Chrome OS'
// shelf, with an overlaid spinner to provide visual feedback.
class ArcShelfSpinnerItemController : public ShelfSpinnerItemController,
                                      public ArcAppListPrefs::Observer,
                                      public arc::ArcSessionManagerObserver {
 public:
  ArcShelfSpinnerItemController(const std::string& arc_app_id,
                                apps::IntentPtr intent,
                                int event_flags,
                                arc::UserInteractionType user_interaction_type,
                                arc::mojom::WindowInfoPtr window_info);

  ArcShelfSpinnerItemController(const ArcShelfSpinnerItemController&) = delete;
  ArcShelfSpinnerItemController& operator=(
      const ArcShelfSpinnerItemController&) = delete;

  ~ArcShelfSpinnerItemController() override;

  // ShelfSpinnerItemController:
  void SetHost(const base::WeakPtr<ShelfSpinnerController>& host) override;

  // ash::ShelfItemDelegate overrides:
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ash::ShelfLaunchSource source,
                    ItemSelectedCallback callback,
                    const ItemFilterPredicate& filter_predicate) override;

  // ArcAppListPrefs::Observer:
  void OnAppStatesChanged(const std::string& app_id,
                          const ArcAppListPrefs::AppInfo& app_info) override;
  void OnAppRemoved(const std::string& removed_app_id) override;
  void OnAppConnectionReady() override;

  // arc::ArcSessionManagerObserver:
  void OnArcPlayStoreEnabledChanged(bool enabled) override;

 private:
  // Returns true if this item is created by full restore. Otherwise, returns
  // false.
  bool IsCreatedByFullRestore();

  apps::IntentPtr intent_;

  // The flags of the event that caused the ARC app to be activated. These will
  // be propagated to the launch event once the app is actually launched.
  const int event_flags_;

  // Stores how this action was initiated.
  const arc::UserInteractionType user_interaction_type_;

  // Time when this controller item was created.
  base::TimeTicks request_time_;

  arc::mojom::WindowInfoPtr window_info_;

  // Unowned
  raw_ptr<Profile> observed_profile_ = nullptr;

  // A one shot timer to close this item.
  std::unique_ptr<base::OneShotTimer> close_timer_;

  base::WeakPtrFactory<ArcShelfSpinnerItemController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_ARC_SHELF_SPINNER_ITEM_CONTROLLER_H_
