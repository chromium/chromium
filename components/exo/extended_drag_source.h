// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_EXTENDED_DRAG_SOURCE_H_
#define COMPONENTS_EXO_EXTENDED_DRAG_SOURCE_H_

#include <string>

#include "base/observer_list.h"
#include "base/optional.h"
#include "components/exo/data_source_observer.h"
#include "ui/gfx/geometry/vector2d.h"

namespace exo {

class DataSource;
class Seat;
class Surface;

class ExtendedDragSource : public DataSourceObserver {
 public:
  class Delegate {
   public:
    virtual bool ShouldAllowDropAnywhere() const = 0;
    virtual bool ShouldLockCursor() const = 0;
    virtual void OnSwallowed(std::string mime_type) = 0;
    virtual void OnUnswallowed(std::string mime_type,
                               const gfx::Vector2d& offset) = 0;
    virtual void OnDataSourceDestroying() = 0;

   protected:
    virtual ~Delegate() = default;
  };

  class Observer {
   public:
    virtual void OnExtendedDragSourceDestroying(ExtendedDragSource* source) = 0;
    virtual void OnDraggedSurfaceChanged(ExtendedDragSource* source) = 0;

   protected:
    virtual ~Observer() = default;
  };

  ExtendedDragSource(DataSource* source, Seat* seat, Delegate* delegate);
  ExtendedDragSource(const ExtendedDragSource&) = delete;
  ExtendedDragSource& operator=(const ExtendedDragSource&) = delete;
  ~ExtendedDragSource() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool should_allow_drop_anywhere() const {
    return delegate_->ShouldAllowDropAnywhere();
  }
  bool should_lock_cursor() const { return delegate_->ShouldLockCursor(); }

  const gfx::Vector2d& drag_offset() const { return drag_offset_; }

  void Drag(Surface* surface, const gfx::Vector2d& offset);

 private:
  // DataSourceObserver:
  void OnDataSourceDestroying(DataSource* source) override;

  // Created and destroyed at wayland/zcr_extended_drag.cc and its lifetime is
  // tied to the zcr_extended_drag_source_v1 object it's attached to.
  Delegate* const delegate_;

  Seat* const seat_;
  DataSource* source_ = nullptr;
  Surface* dragged_surface_ = nullptr;
  gfx::Vector2d drag_offset_;
  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_EXTENDED_DRAG_SOURCE_H_
