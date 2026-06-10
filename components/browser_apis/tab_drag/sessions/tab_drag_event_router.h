// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_EVENT_ROUTER_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_EVENT_ROUTER_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_input_listener.h"
#include "components/browser_apis/tab_drag/tab_drag_api.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace tabs_api {

class TabDragWindowAdapter;
class TabDragSession;

// Routes tab drag events to the right DropTarget. It maintains a mapping of
// active drop targets associated with their respective browser windows.
class TabDragEventRouter : public TabDragSessionInputListener {
 public:
  TabDragEventRouter();
  TabDragEventRouter(const TabDragEventRouter&) = delete;
  TabDragEventRouter& operator=(const TabDragEventRouter&) = delete;
  ~TabDragEventRouter() override;

  // Registers a `target` (DropTarget) associated with the given
  // `window_adapter`. The `window_adapter` is used as a key to route events to
  // the correct target based on the window coordinates. The `registration`
  // receiver is used to manage the lifetime of the registration; when the
  // client discards the corresponding remote, the target will be automatically
  // unregistered.
  void RegisterDropTarget(
      TabDragWindowAdapter* window_adapter,
      mojo::PendingAssociatedRemote<mojom::DropTarget> target,
      mojo::PendingAssociatedReceiver<mojom::DropTargetRegistration>
          registration);

  // Unregisters the drop target associated with the given `window_adapter`.
  // This is typically called automatically when the `DropTargetRegistration`
  // pipe is closed.
  void UnregisterDropTarget(TabDragWindowAdapter* window_adapter);

  // TabDragSessionInputListener overrides:
  void OnSessionStarted(TabDragSession* session) override;
  void OnSessionEnded() override;
  void OnDragSessionEvent(const TabDragSessionInputEvent& event) override;

  base::WeakPtr<TabDragEventRouter> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  std::map<TabDragWindowAdapter*, mojo::AssociatedRemote<mojom::DropTarget>>
      drop_targets_;
  raw_ptr<TabDragSession> active_session_ = nullptr;
  base::WeakPtrFactory<TabDragEventRouter> weak_factory_{this};
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_EVENT_ROUTER_H_
