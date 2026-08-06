// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/sessions/tab_drag_session_manager.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"
#include "components/browser_apis/tab_drag/destinations/drop_target_registry.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_injector.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_window_registry.h"
#include "mojo/public/mojom/base/error.mojom.h"

namespace tabs_api {

TabDragSessionManager::TabDragSessionManager(
    std::unique_ptr<TabDragSessionInjector> injector)
    : injector_(std::move(injector)) {
  CHECK(injector_);
}

TabDragSessionManager::~TabDragSessionManager() = default;

base::expected<std::monostate, mojo_base::mojom::ErrorPtr>
TabDragSessionManager::StartDrag(
    TabDragWindowAdapter* source_window,
    const std::vector<tabs_api::NodeId>& source_tab_ids,
    const gfx::Point& start_point,
    int32_t tab_original_offset_x) {
  if (source_tab_ids.empty()) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInvalidArgument, "source tabs are empty"));
  }

  if (active_session_) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kFailedPrecondition,
        "drag session is already active"));
  }

  TabDragSessionParams params;
  params.source_window_id = source_window->GetWindowId();
  params.source_tab_ids = source_tab_ids;
  params.start_point = start_point;
  params.tab_original_offset_x = tab_original_offset_x;
  params.end_callback = base::BindOnce(&TabDragSessionManager::OnSessionEnded,
                                       weak_factory_.GetWeakPtr());

  active_session_ =
      std::make_unique<TabDragSession>(std::move(params), injector_.get());

  auto start_result = active_session_->Start();
  if (!start_result.has_value()) {
    active_session_.reset();
    return base::unexpected(std::move(start_result.error()));
  }

  return std::monostate();
}

void TabDragSessionManager::OnSessionEnded() {
  if (active_session_) {
    // Post a task to destroy the active session. This keeps the session pointer
    // valid (and prevents a new drag from starting) until the old session is
    // actually destroyed (and capture is released) returning to the message
    // loop.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&TabDragSessionManager::DestroyActiveSession,
                                  weak_factory_.GetWeakPtr()));
  }
}

void TabDragSessionManager::DestroyActiveSession() {
  if (active_session_) {
    active_session_.reset();
  }
}

DropTargetRegistry& TabDragSessionManager::GetDropTargetRegistry() {
  return injector_->GetDropTargetRegistry();
}

TabDragWindowRegistry* TabDragSessionManager::GetWindowRegistry() {
  return injector_->GetWindowRegistry();
}

}  // namespace tabs_api
