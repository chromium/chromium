// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/destinations/drop_target_registry_impl.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"
#include "components/browser_apis/tab_drag/destinations/drop_target.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"

namespace tabs_api {

class DropTargetRegistrationMojoImpl : public mojom::DropTargetRegistration {
 public:
  DropTargetRegistrationMojoImpl(base::WeakPtr<DropTargetRegistryImpl> registry,
                                 DropTargetId target_id)
      : registry_(registry), target_id_(target_id) {}

  ~DropTargetRegistrationMojoImpl() override {
    if (registry_) {
      registry_->UnregisterDropTarget(target_id_);
    }
  }

  // mojom::DropTargetRegistration:
  void OnBoundsChanged(const gfx::Rect& bounds) override {
    if (registry_) {
      registry_->UpdateTargetBounds(target_id_, bounds);
    }
  }

 private:
  base::WeakPtr<DropTargetRegistryImpl> registry_;
  DropTargetId target_id_;
};

DropTargetRegistryImpl::DropTargetRegistryImpl() = default;
DropTargetRegistryImpl::~DropTargetRegistryImpl() = default;

DropTargetId DropTargetRegistryImpl::RegisterDropTarget(
    TabDragWindowAdapter* window,
    gfx::NativeView native_view,
    mojo::PendingAssociatedRemote<mojom::DropTarget> target,
    mojo::PendingAssociatedReceiver<mojom::DropTargetRegistration>
        registration) {
  CHECK(window);
  DropTargetId id = id_generator_.GenerateNextId();
  drop_targets_[id] =
      std::make_unique<DropTarget>(id, window, native_view, std::move(target));

  mojo::MakeSelfOwnedAssociatedReceiver(
      std::make_unique<DropTargetRegistrationMojoImpl>(AsWeakPtr(), id),
      std::move(registration));
  return id;
}

void DropTargetRegistryImpl::UnregisterDropTarget(DropTargetId target_id) {
  drop_targets_.erase(target_id);
}

DropTargetId DropTargetRegistryImpl::FindTargetAtPoint(
    const gfx::Point& screen_point,
    DropTargetId exclude_target) const {
  for (const auto& [target_id, target] : drop_targets_) {
    if (target_id == exclude_target) {
      continue;
    }
    TabDragWindowAdapter* window_adapter = target->window();
    if (!window_adapter) {
      continue;
    }

    // Pass the target's native_view for coordinate conversion
    gfx::Point local_point = window_adapter->ConvertScreenPointToLocal(
        target->native_view(), screen_point);
    auto bounds_opt = target->cached_bounds();
    if (bounds_opt) {
      if (bounds_opt->Contains(local_point)) {
        return target_id;
      }
    }
  }
  return DropTargetId();
}

DropTargetId DropTargetRegistryImpl::FindTargetForWindow(
    TabDragWindowId window_id) const {
  for (const auto& [target_id, target] : drop_targets_) {
    if (target->window_id() == window_id) {
      return target_id;
    }
  }
  return DropTargetId();
}

DropTarget* DropTargetRegistryImpl::GetDropTarget(
    DropTargetId target_id) const {
  auto it = drop_targets_.find(target_id);
  if (it != drop_targets_.end()) {
    return it->second.get();
  }
  return nullptr;
}

std::optional<gfx::Rect> DropTargetRegistryImpl::GetCachedBounds(
    DropTargetId target_id) const {
  auto* target = GetDropTarget(target_id);
  return target ? target->cached_bounds() : std::nullopt;
}

void DropTargetRegistryImpl::UpdateTargetBounds(DropTargetId target_id,
                                                const gfx::Rect& bounds) {
  auto* target = GetDropTarget(target_id);
  if (target) {
    target->set_cached_bounds(bounds);
  }
}

}  // namespace tabs_api
