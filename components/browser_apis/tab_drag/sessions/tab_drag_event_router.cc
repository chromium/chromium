// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/sessions/tab_drag_event_router.h"

#include <memory>

#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"

namespace tabs_api {

class DropTargetRegistrationImpl : public mojom::DropTargetRegistration {
 public:
  DropTargetRegistrationImpl(base::WeakPtr<TabDragEventRouter> router,
                             base::WeakPtr<TabDragWindowAdapter> window_adapter)
      : router_(router), window_adapter_(window_adapter) {}

  ~DropTargetRegistrationImpl() override {
    if (router_ && window_adapter_) {
      router_->UnregisterDropTarget(window_adapter_.get());
    }
  }

 private:
  base::WeakPtr<TabDragEventRouter> router_;
  base::WeakPtr<TabDragWindowAdapter> window_adapter_;
};

TabDragEventRouter::TabDragEventRouter() = default;
TabDragEventRouter::~TabDragEventRouter() = default;

void TabDragEventRouter::RegisterDropTarget(
    TabDragWindowAdapter* window_adapter,
    mojo::PendingAssociatedRemote<mojom::DropTarget> target,
    mojo::PendingAssociatedReceiver<mojom::DropTargetRegistration>
        registration) {
  drop_targets_[window_adapter] =
      mojo::AssociatedRemote<mojom::DropTarget>(std::move(target));

  mojo::MakeSelfOwnedAssociatedReceiver(
      std::make_unique<DropTargetRegistrationImpl>(AsWeakPtr(),
                                                   window_adapter->AsWeakPtr()),
      std::move(registration));
}

void TabDragEventRouter::UnregisterDropTarget(
    TabDragWindowAdapter* window_adapter) {
  drop_targets_.erase(window_adapter);
}

void TabDragEventRouter::OnSessionStarted(TabDragSession* session) {
  active_session_ = session;
}

void TabDragEventRouter::OnSessionEnded() {
  active_session_ = nullptr;
}

void TabDragEventRouter::OnDragSessionEvent(
    const TabDragSessionInputEvent& event) {
  // TODO(crbug.com/501070453) Implement event routing.
}

}  // namespace tabs_api
