// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_SESSION_MANAGER_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_SESSION_MANAGER_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "mojo/public/mojom/base/error.mojom-forward.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/native_ui_types.h"

namespace tabs_api {

class TabDragPlatformProvider;
class TabDragSession;
class TabDragEventRouter;

// Browser-process-wide manager that owns and coordinates the active
// TabDragSession. This ensures the session outlives individual window
// lifecycles (crucial for tear-off and merge operations).
class TabDragSessionManager {
 public:
  explicit TabDragSessionManager(
      std::unique_ptr<TabDragPlatformProvider> platform_provider);
  TabDragSessionManager(const TabDragSessionManager&) = delete;
  TabDragSessionManager& operator=(const TabDragSessionManager&) = delete;
  ~TabDragSessionManager();

  // Starts a global drag session. Returns monostate if successful, or an error.
  base::expected<std::monostate, mojo_base::mojom::ErrorPtr> StartDrag(
      const std::vector<tabs_api::NodeId>& source_tab_ids,
      const gfx::Point& start_point);

  // Cancels the active session if one exists.
  void CancelDrag();

  TabDragSession* active_session() { return active_session_.get(); }

  // Callback notified by the active session when it naturally terminates.
  void OnSessionEnded();

  TabDragEventRouter* event_router() { return event_router_.get(); }

 private:
  void DestroyActiveSession();

  std::unique_ptr<TabDragPlatformProvider> platform_provider_;
  std::unique_ptr<TabDragSession> active_session_;
  std::unique_ptr<TabDragEventRouter> event_router_;

  base::WeakPtrFactory<TabDragSessionManager> weak_factory_{this};
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_SESSION_MANAGER_H_
