// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/extended_drag_source.h"

#include "base/check_op.h"
#include "base/logging.h"
#include "components/exo/data_source.h"
#include "components/exo/seat.h"
#include "components/exo/surface.h"
#include "ui/gfx/geometry/vector2d.h"

namespace exo {

ExtendedDragSource::ExtendedDragSource(DataSource* source,
                                       Seat* seat,
                                       Delegate* delegate)
    : delegate_(delegate), seat_(seat), source_(source) {
  DCHECK(source_);
  DCHECK(seat_);
  DCHECK(delegate_);

  DVLOG(1) << "ExtendedDragSource created. wl_source=" << source_;
  source_->AddObserver(this);
}

ExtendedDragSource::~ExtendedDragSource() {
  delegate_->OnDataSourceDestroying();
  for (auto& observer : observers_)
    observer.OnExtendedDragSourceDestroying(this);

  if (source_)
    source_->RemoveObserver(this);
}

void ExtendedDragSource::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void ExtendedDragSource::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void ExtendedDragSource::Drag(Surface* dragged_surface,
                              const gfx::Vector2d& drag_offset) {
  // Associated data source already destroyed.
  if (!source_)
    return;

  if (dragged_surface == dragged_surface_ && drag_offset == drag_offset_)
    return;

  dragged_surface_ = dragged_surface;
  drag_offset_ = drag_offset;
  DVLOG(1) << "Dragged surface changed: surface=" << dragged_surface_
           << " offset=" << drag_offset_.ToString();

  for (auto& observer : observers_)
    observer.OnDraggedSurfaceChanged(this);
}

void ExtendedDragSource::OnDataSourceDestroying(DataSource* source) {
  DCHECK_EQ(source, source_);
  source_->RemoveObserver(this);
  source_ = nullptr;
}

}  // namespace exo
