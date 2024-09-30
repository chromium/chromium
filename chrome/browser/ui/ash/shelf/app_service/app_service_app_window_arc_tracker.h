// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_APP_SERVICE_APP_WINDOW_ARC_TRACKER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_APP_SERVICE_APP_WINDOW_ARC_TRACKER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ash/components/arc/arc_util.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"

namespace arc {
class ArcAppShelfId;
}

namespace ash {
class ShelfItemDelegate;
}

namespace aura {
class window;
}

namespace gfx {
class ImageSkia;
}

class AppServiceAppWindowShelfController;
class AppServiceAppWindowShelfItemController;
class ArcAppWindowInfo;
class Profile;

// AppServiceAppWindowArcTracker observes the ArcAppListPrefs to handle ARC app
// window special cases, e.g. task id, closing ARC app windows, etc.
class AppServiceAppWindowArcTracker : public ArcAppListPrefs::Observer,
                                      public arc::ArcSessionManagerObserver {
 public:
  explicit AppServiceAppWindowArcTracker(
      AppServiceAppWindowShelfController* app_service_controller);
  ~AppServiceAppWindowArcTracker() override;

  AppServiceAppWindowArcTracker(const AppServiceAppWindowArcTracker&) = delete;
  AppServiceAppWindowArcTracker& operator=(
      const AppServiceAppWindowArcTracker&) = delete;

  void ActiveUserChanged(const std::string& user_email);

  // Invoked by controller to notify |window| visibility is changed.
  void HandleWindowVisibilityChanged(aura::Window* window);

  // Invoked by controller to notify |window| activated is changed.
  void HandleWindowActivatedChanged(aura::Window* window);

  // Invoked by controller to notify |window| is destroying.
  void HandleWindowDestroying(aura::Window* window);

  // Close all windows for 'app_id'.
  void CloseWindows(const std::string& app_id);

  // Invoked by controller to notify |window| may be replaced from ghost window
  // to app window.
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old);

  // ArcAppListPrefs::Observer:
  void OnAppStatesChanged(const std::string& app_id,
                          const ArcAppListPrefs::AppInfo& app_info) override;
  void OnAppRemoved(const std::string& app_id) override;
  void OnTaskCreated(int32_t task_id,
                     const std::string& package_name,
                     const std::string& activity,
                     const std::string& intent,
                     int32_t session_id) override;
  void OnTaskDescriptionChanged(int32_t task_id,
                                const std::string& label,
                                const arc::mojom::RawIconPngData& icon,
                                uint32_t primary_color,
                                uint32_t status_bar_color) override;
  void OnTaskDestroyed(int32_t task_id) override;
  void OnTaskSetActive(int32_t task_id) override;

  // Attaches controller and sets window's property when |window| is an ARC
  // window and has the related task id.
  void AttachControllerToWindow(aura::Window* window);

  // Adds the app window to |arc_window_candidates_|.
  void AddCandidateWindow(aura::Window* window);
  // Removes the app window from |arc_window_candidates_|.
  void RemoveCandidateWindow(aura::Window* window);

  // Removes controller from |app_shelf_group_to_controller_map_|.
  void OnItemDelegateDiscarded(const ash::ShelfID& shelf_id,
                               ash::ShelfItemDelegate* delegate);

  ash::ShelfID GetShelfId(aura::Window* window);

  int active_task_id() const { return active_task_id_; }

  int active_session_id() const { return active_session_id_; }

 private:
  using TaskIdToArcAppWindowInfo =
      std::map<int, std::unique_ptr<ArcAppWindowInfo>>;

  using SessionIdToArcAppWindowInfo =
      std::map<int, std::unique_ptr<ArcAppWindowInfo>>;

  // Maps shelf group id to controller. Shelf group id is optional parameter for
  // the Android task. If it is not set, app id is used instead.
  using ShelfGroupToAppControllerMap = std::map<
      arc::ArcAppShelfId,
      raw_ptr<AppServiceAppWindowShelfItemController, CtnExperimental>>;

  // Checks |arc_window_candidates_| and attaches controller when they
  // are ARC app windows and have task id or session id.
  void CheckAndAttachControllers();
  void AttachControllerToTask(int taskId);
  void AttachControllerToSession(int session_id, const ArcAppWindowInfo& info);

  // arc::ArcSessionManagerObserver:
  void OnArcPlayStoreEnabledChanged(bool enabled) override;

  // Returns a task ID different from |task_id| that is part of the same
  // logical window. Return arc::kNoTaskId if there is no such window.
  // For consistency, always return the lowest such task ID.
  int GetTaskIdSharingLogicalWindow(int task_id);

  std::vector<int> GetTaskIdsForApp(const std::string& app_id) const;

  // Returns session ids of all ghost windows for the app of `arc_app_id`.
  std::vector<int> GetSessionIdsForApp(const std::string& app_id) const;

  // Invoked when the compressed data is converted to an ImageSkia.
  void OnIconLoaded(int32_t task_id,
                    const std::string& title,
                    const gfx::ImageSkia& icon);

  ArcAppWindowInfo* GetArcAppWindowInfo(aura::Window* window);

  // Invoked when the app is removed to close the ghost window with
  // `session_id`.
  void OnSessionDestroyed(int32_t session_id);

  const raw_ptr<Profile> observed_profile_;
  const raw_ptr<AppServiceAppWindowShelfController> app_service_controller_;

  TaskIdToArcAppWindowInfo task_id_to_arc_app_window_info_;
  SessionIdToArcAppWindowInfo session_id_to_arc_app_window_info_;
  ShelfGroupToAppControllerMap app_shelf_group_to_controller_map_;

  // Temporarily map session id to task id, starting from OnTaskCreated called
  // to exo application id set (until that `arc::GetWindowTaskId` can return
  // correct task id for window, or it will still be session id).
  std::map<int, int> session_id_to_task_id_map_;

  // ARC app task id could be created after the window initialized.
  // |arc_window_candidates_| is used to record those initialized ARC app
  // windows, which haven't been assigned a task id. When a task id is created,
  // the windows in |arc_window_candidates_| will be checked and attach the task
  // id. Once the window is assigned a task id, the window is removed from
  // |arc_window_candidates_|.
  std::set<raw_ptr<aura::Window, SetExperimental>> arc_window_candidates_;

  int active_task_id_ = arc::kNoTaskId;
  int active_session_id_ = arc::kNoTaskId;

  // TODO(crbug.com/40808991): A temp variable used to investigate whether
  // OnTaskDestroyed is called in the middle of OnTaskCreated. This can be
  // removed if we have the result.
  int task_id_being_created_ = arc::kNoTaskId;

  base::WeakPtrFactory<AppServiceAppWindowArcTracker> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_APP_SERVICE_APP_WINDOW_ARC_TRACKER_H_
