// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_DRAG_DROP_OPERATION_H_
#define COMPONENTS_EXO_DRAG_DROP_OPERATION_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "components/exo/data_device.h"
#include "components/exo/data_offer_observer.h"
#include "components/exo/data_source_observer.h"
#include "components/exo/extended_drag_source.h"
#include "components/exo/surface_observer.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/client/drag_drop_client_observer.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/gfx/geometry/point_f.h"

class SkBitmap;

namespace ash {
class DragDropController;
}  // namespace ash

namespace ui {
class OSExchangeData;
}

namespace exo {
class DataExchangeDelegate;
class ScopedDataSource;
class Surface;
class ScopedSurface;

// This class represents an ongoing drag-drop operation started by an exo
// client. It manages its own lifetime. It will delete itself when the drag
// operation completes, is cancelled, or some vital resource is destroyed
// (e.g. the client deletes the data source used to start the drag operation),
// or if another drag operation races with this one to start and wins.
class DragDropOperation : public DataSourceObserver,
                          public SurfaceObserver,
                          public ExtendedDragSource::Observer,
                          public aura::client::DragDropClientObserver {
 public:
  // Create an operation for a drag-drop originating from a wayland app.
  static base::WeakPtr<DragDropOperation> Create(
      DataExchangeDelegate* data_exchange_delegate,
      DataSource* source,
      Surface* origin,
      Surface* icon,
      const gfx::PointF& drag_start_point,
      ui::mojom::DragEventSource event_source);

  DragDropOperation(const DragDropOperation&) = delete;
  DragDropOperation& operator=(const DragDropOperation&) = delete;

  // Abort the operation if it hasn't been started yet, otherwise do nothing.
  void AbortIfPending();

  // The drag drop has started.
  bool started() const { return started_; }

  // DataSourceObserver:
  void OnDataSourceDestroying(DataSource* source) override;

  // SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override;

  // aura::client::DragDropClientObserver:
  void OnDragStarted() override;
  void OnDragActionsChanged(int actions) override;

  // ExtendedDragSource::Observer:
  void OnExtendedDragSourceDestroying(ExtendedDragSource* source) override;

 private:
  class IconSurface;

  // A private constructor and destructor are used to prevent anyone else from
  // attempting to manage the lifetime of a DragDropOperation.
  DragDropOperation(DataExchangeDelegate* data_exchange_delegate,
                    DataSource* source,
                    Surface* origin,
                    Surface* icon,
                    const gfx::PointF& drag_start_point,
                    ui::mojom::DragEventSource event_source);
  ~DragDropOperation() override;

  void OnDragIconCaptured(const SkBitmap& icon_bitmap);

  // Called when the focused window is a Lacros window and a source
  // DataTransferEndpoint is found in the available MIME types. This
  // is currently used to synchronize drag source metadata from
  // Lacros to Ash.
  void OnDataTransferEndpointRead(const std::string& mime_type,
                                  std::u16string data);

  void OnTextRead(const std::string& mime_type, std::u16string data);
  void OnHTMLRead(const std::string& mime_type, std::u16string data);
  void OnFilenamesRead(DataExchangeDelegate* data_exchange_delegate,
                       aura::Window* source,
                       const std::string& mime_type,
                       const std::vector<uint8_t>& data);
  void OnFileContentsRead(const std::string& mime_type,
                          const base::FilePath& filename,
                          const std::vector<uint8_t>& data);
  void OnWebCustomDataRead(const std::string& mime_type,
                           const std::vector<uint8_t>& data);

  void ScheduleStartDragDropOperation();

  // This operation triggers a nested RunLoop, and should not be called
  // directly. Use ScheduleStartDragDropOperation instead.
  void StartDragDropOperation();

  void ResetExtendedDragSource();

  std::unique_ptr<ScopedDataSource> source_;
  std::unique_ptr<ScopedSurface> icon_;
  std::unique_ptr<ScopedSurface> origin_;
  gfx::PointF drag_start_point_;
  std::unique_ptr<ui::OSExchangeData> os_exchange_data_;
  raw_ptr<ash::DragDropController> drag_drop_controller_;

  base::RepeatingClosure counter_;

  // Stores whether this object has just started a drag operation. If so, we
  // want to ignore the OnDragStarted event, and self destruct the object when
  // completed.
  bool started_ = false;

  bool captured_icon_ = false;

  // TODO(crbug.com/994065) This is currently not the actual mime type used by
  // the recipient, just an arbitrary one we pick out of the offered types so we
  // can report back whether or not the drop can succeed. This may need to
  // change in the future.
  std::string mime_type_;

  ui::mojom::DragEventSource event_source_;

  raw_ptr<ExtendedDragSource> extended_drag_source_;

  // TODO(crbug.com/40061238): Remove this once the issue is fixed.
  base::OneShotTimer start_drag_drop_timer_;
  void DragDataReadTimeout();

  base::WeakPtrFactory<DragDropOperation> weak_ptr_factory_{this};
};

}  // namespace exo

#endif  // COMPONENTS_EXO_DRAG_DROP_OPERATION_H_
