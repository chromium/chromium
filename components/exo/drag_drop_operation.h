// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_DRAG_DROP_OPERATION_H_
#define COMPONENTS_EXO_DRAG_DROP_OPERATION_H_

#include "components/exo/data_device.h"
#include "components/exo/data_offer_observer.h"
#include "components/exo/data_source_observer.h"
#include "components/exo/surface_observer.h"
#include "components/exo/surface_tree_host.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/client/drag_drop_client_observer.h"
#include "ui/base/dragdrop/drag_drop_types.h"

namespace ash {
class DragDropController;
}  // namespace ash

namespace aura {
namespace client {
class DragDropClient;
}  // namespace client
}  // namespace aura

namespace ui {
class OSExchangeData;
}

namespace viz {
class CopyOutputResult;
}

namespace exo {
class ScopedDataSource;

// This class represents an ongoing drag-drop operation started by an exo
// client. It manages its own lifetime. It will delete itself when the drag
// operation completes, is cancelled, or some vital resource is destroyed
// (e.g. the client deletes the data source used to start the drag operation),
// or if another drag operation races with this one to start and wins.
class DragDropOperation : public DataSourceObserver,
                          public SurfaceTreeHost,
                          public SurfaceObserver,
                          public aura::client::DragDropClientObserver {
 public:
  // Create an operation for a drag-drop originating from a wayland app.
  static base::WeakPtr<DragDropOperation> Create(
      DataSource* source,
      Surface* origin,
      Surface* icon,
      ui::DragDropTypes::DragEventSource event_source);

  // Abort the operation if it hasn't been started yet, otherwise do nothing.
  void AbortIfPending();

  // DataSourceObserver:
  void OnDataSourceDestroying(DataSource* source) override;

  // SurfaceDelegate:
  void OnSurfaceCommit() override;

  // SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override;

  // aura::client::DragDropClientObserver:
  void OnDragStarted() override;
  void OnDragEnded() override;
#if defined(OS_CHROMEOS)
  void OnDragActionsChanged(int actions) override;
#endif

 private:
  // A private constructor and destructor are used to prevent anyone else from
  // attempting to manage the lifetime of a DragDropOperation.
  DragDropOperation(DataSource* source,
                    Surface* origin,
                    Surface* icon,
                    ui::DragDropTypes::DragEventSource event_source);
  ~DragDropOperation() override;

  void CaptureDragIcon();
  void OnDragIconCaptured(std::unique_ptr<viz::CopyOutputResult> icon_result);

  void OnTextRead(const std::string& mime_type, base::string16 data);
  void OnHTMLRead(const std::string& mime_type, base::string16 data);

  void ScheduleStartDragDropOperation();

  // This operation triggers a nested RunLoop, and should not be called
  // directly. Use ScheduleStartDragDropOperation instead.
  void StartDragDropOperation();

  std::unique_ptr<ScopedDataSource> source_;
  std::unique_ptr<ScopedSurface> icon_;
  std::unique_ptr<ScopedSurface> origin_;
  gfx::Point drag_start_point_;
  std::unique_ptr<ui::OSExchangeData> os_exchange_data_;
#if defined(OS_CHROMEOS)
  ash::DragDropController* drag_drop_controller_;
#else
  aura::client::DragDropClient* drag_drop_controller_;
#endif  // defined(OS_CHROMEOS)

  base::RepeatingClosure counter_;

  // Stores whether this object has just started a drag operation. If so, we
  // want to ignore the OnDragStarted event.
  bool started_by_this_object_ = false;

  bool captured_icon_ = false;

  // TODO(crbug.com/994065) This is currently not the actual mime type used by
  // the recipient, just an arbitrary one we pick out of the offered types so we
  // can report back whether or not the drop can succeed. This may need to
  // change in the future.
  std::string mime_type_;

  ui::DragDropTypes::DragEventSource event_source_;

  base::WeakPtrFactory<DragDropOperation> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DragDropOperation);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_DRAG_DROP_OPERATION_H_
