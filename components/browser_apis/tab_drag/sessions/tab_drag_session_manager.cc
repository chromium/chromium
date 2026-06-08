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
#include "components/browser_apis/tab_drag/sessions/tab_drag_session.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "mojo/public/mojom/base/error.mojom.h"

namespace tabs_api {

class DropTargetRegistrationImpl : public mojom::DropTargetRegistration {
 public:
  DropTargetRegistrationImpl(base::WeakPtr<TabDragSessionManager> manager,
                             base::WeakPtr<TabDragWindowAdapter> window_adapter)
      : manager_(manager), window_adapter_(window_adapter) {}

  ~DropTargetRegistrationImpl() override {
    if (manager_ && window_adapter_) {
      manager_->UnregisterDropTarget(window_adapter_.get());
    }
  }

 private:
  base::WeakPtr<TabDragSessionManager> manager_;
  base::WeakPtr<TabDragWindowAdapter> window_adapter_;
};

TabDragSessionManager::TabDragSessionManager(
    std::unique_ptr<TabDragPlatformProvider> platform_provider)
    : platform_provider_(std::move(platform_provider)) {
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
      platform_provider_->tab_drag_session_input_adapter(),
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
  active_session_.reset();
}

void TabDragSessionManager::RegisterDropTarget(
    TabDragWindowAdapter* window_adapter,
    mojo::PendingAssociatedRemote<mojom::DropTarget> target,
    mojo::PendingAssociatedReceiver<mojom::DropTargetRegistration>
        registration) {
  drop_targets_[window_adapter] =
      mojo::AssociatedRemote<mojom::DropTarget>(std::move(target));

  mojo::MakeSelfOwnedAssociatedReceiver(
      std::make_unique<DropTargetRegistrationImpl>(weak_factory_.GetWeakPtr(),
                                                   window_adapter->AsWeakPtr()),
      std::move(registration));
}

void TabDragSessionManager::UnregisterDropTarget(
    TabDragWindowAdapter* window_adapter) {
  drop_targets_.erase(window_adapter);
}

}  // namespace tabs_api
