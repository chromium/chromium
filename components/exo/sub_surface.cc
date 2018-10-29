// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/sub_surface.h"

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "components/exo/surface.h"

namespace exo {

////////////////////////////////////////////////////////////////////////////////
// SubSurface, public:

SubSurface::SubSurface(Surface* surface, Surface* parent)
    : surface_(surface), parent_(parent) {
  surface_->SetSurfaceDelegate(this);
  surface_->AddSurfaceObserver(this);
  parent_->AddSurfaceObserver(this);
  parent_->AddSubSurface(surface_);
}

SubSurface::~SubSurface() {
  if (surface_) {
    if (parent_)
      parent_->RemoveSubSurface(surface_);
    surface_->SetSurfaceDelegate(nullptr);
    surface_->RemoveSurfaceObserver(this);
  }
  if (parent_)
    parent_->RemoveSurfaceObserver(this);
}

void SubSurface::SetPosition(const gfx::Point& position) {
  TRACE_EVENT1("exo", "SubSurface::SetPosition", "position",
               position.ToString());

  if (!parent_ || !surface_)
    return;

  parent_->SetSubSurfacePosition(surface_, position);
}

void SubSurface::PlaceAbove(Surface* reference) {
  TRACE_EVENT1("exo", "SubSurface::PlaceAbove", "reference",
               reference->AsTracedValue());

  if (!parent_ || !surface_)
    return;

  if (reference == surface_) {
    DLOG(WARNING) << "Client tried to place sub-surface above itself";
    return;
  }

  parent_->PlaceSubSurfaceAbove(surface_, reference);
}

void SubSurface::PlaceBelow(Surface* sibling) {
  TRACE_EVENT1("exo", "SubSurface::PlaceBelow", "sibling",
               sibling->AsTracedValue());

  if (!parent_ || !surface_)
    return;

  if (sibling == surface_) {
    DLOG(WARNING) << "Client tried to place sub-surface below itself";
    return;
  }

  parent_->PlaceSubSurfaceBelow(surface_, sibling);
}

void SubSurface::SetCommitBehavior(bool synchronized) {
  TRACE_EVENT1("exo", "SubSurface::SetCommitBehavior", "synchronized",
               synchronized);

  is_synchronized_ = synchronized;
}

std::unique_ptr<base::trace_event::TracedValue> SubSurface::AsTracedValue()
    const {
  std::unique_ptr<base::trace_event::TracedValue> value(
      new base::trace_event::TracedValue());
  value->SetBoolean("is_synchronized", is_synchronized_);
  return value;
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceDelegate overrides:

void SubSurface::OnSurfaceCommit() {
  // Early out if commit should be synchronized with parent.
  if (IsSurfaceSynchronized())
    return;

  if (parent_)
    parent_->OnSubSurfaceCommit();
}

bool SubSurface::IsSurfaceSynchronized() const {
  // A sub-surface is effectively synchronized if either its parent is
  // synchronized or itself is in synchronized mode.
  if (is_synchronized_)
    return true;

  return parent_ && parent_->IsSynchronized();
}

bool SubSurface::IsInputEnabled(Surface* surface) const {
  return !parent_ || parent_->IsInputEnabled(surface);
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceObserver overrides:

void SubSurface::OnSurfaceDestroying(Surface* surface) {
  surface->RemoveSurfaceObserver(this);
  if (surface == parent_) {
    parent_ = nullptr;
    return;
  }
  DCHECK(surface == surface_);
  if (parent_)
    parent_->RemoveSubSurface(surface_);
  surface_ = nullptr;
}

}  // namespace exo
