// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/sessions/tab_drag_session_manager.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_platform_provider.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_session_input_adapter.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_event_router.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session.h"
#include "mojo/public/mojom/base/error.mojom.h"

namespace tabs_api {

TabDragSessionManager::TabDragSessionManager(
    std::unique_ptr<TabDragPlatformProvider> platform_provider)
    : platform_provider_(std::move(platform_provider)),
      event_router_(std::make_unique<TabDragEventRouter>()) {
  CHECK(platform_provider_);
}

TabDragSessionManager::~TabDragSessionManager() = default;

base::expected<std::monostate, mojo_base::mojom::ErrorPtr>
TabDragSessionManager::StartDrag(
    const std::vector<tabs_api::NodeId>& source_tab_ids,
    const gfx::Point& start_point) {
  if (source_tab_ids.empty()) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInvalidArgument, "source tabs are empty"));
  }

  if (active_session_) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kFailedPrecondition,
        "drag session is already active"));
  }

  auto session = std::make_unique<TabDragSession>(
      source_tab_ids, start_point,
      platform_provider_->tab_drag_session_input_adapter(), event_router_.get(),
      base::BindOnce(&TabDragSessionManager::OnSessionEnded,
                     weak_factory_.GetWeakPtr()));

  auto start_result = session->Start();
  if (!start_result.has_value()) {
    return base::unexpected(std::move(start_result.error()));
  }

  active_session_ = std::move(session);
  return std::monostate();
}

void TabDragSessionManager::CancelDrag() {
  if (active_session_) {
    active_session_->Cancel();
  }
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

}  // namespace tabs_api
