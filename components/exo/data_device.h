// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_DATA_DEVICE_H_
#define COMPONENTS_EXO_DATA_DEVICE_H_

#include <cstdint>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/exo/data_offer_observer.h"
#include "components/exo/seat_observer.h"
#include "components/exo/surface.h"
#include "components/exo/surface_observer.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"

namespace ui {
class DropTargetEvent;
}

namespace exo {

class DataDeviceDelegate;
class DataOffer;
class ScopedDataOffer;
class DataSource;
class Seat;
class ScopedSurface;

enum class DndAction { kNone, kCopy, kMove, kAsk };

// DataDevice to start drag and drop and copy and paste operations.
class DataDevice : public DataOfferObserver,
                   public aura::client::DragDropDelegate,
                   public ui::ClipboardObserver,
                   public SurfaceObserver,
                   public SeatObserver {
 public:
  DataDevice(DataDeviceDelegate* delegate, Seat* seat);

  DataDevice(const DataDevice&) = delete;
  DataDevice& operator=(const DataDevice&) = delete;

  ~DataDevice() override;

  // Starts drag-and-drop operation.
  // |source| represents data comes from the client starting drag operation. Can
  // be null if the data will be transferred only in the client.  |origin| is
  // the surface which starts the drag and drop operation. |icon| is the
  // nullable image which is rendered at the next to cursor while drag
  // operation.
  void StartDrag(DataSource* source,
                 Surface* origin,
                 Surface* icon,
                 ui::mojom::DragEventSource event_source);

  // Sets selection data to the clipboard.
  // |source| represents data comes from the client.
  void SetSelection(DataSource* source);

  // aura::client::DragDropDelegate:
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  aura::client::DragUpdateInfo OnDragUpdated(
      const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  aura::client::DragDropDelegate::DropCallback GetDropCallback(
      const ui::DropTargetEvent& event) override;

  // Overridden from ui::ClipboardObserver:
  void OnClipboardDataChanged() override;

  // Overridden from SeatObserver:
  void OnSurfaceCreated(Surface* surface) override;
  void OnSurfaceFocused(Surface* surface,
                        Surface* lost_focus,
                        bool has_focused_client) override;

  // Overridden from DataOfferObserver:
  void OnDataOfferDestroying(DataOffer* data_offer) override;

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override;

  DataDeviceDelegate* get_delegate() { return delegate_; }

 private:
  Surface* GetEffectiveTargetForEvent(const ui::DropTargetEvent& event) const;
  void SetSelectionToCurrentClipboardData();

  void PerformDropOrExitDrag(
      base::ScopedClosureRunner exit_drag,
      std::unique_ptr<ui::OSExchangeData> data,
      ui::mojom::DragOperation& output_drag_op,
      std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner);

  const raw_ptr<DataDeviceDelegate, DanglingUntriaged> delegate_;
  const raw_ptr<Seat> seat_;
  std::unique_ptr<ScopedDataOffer> data_offer_;
  std::unique_ptr<ScopedSurface> focused_surface_;

  base::OnceClosure quit_closure_;
  bool drop_succeeded_;
  base::WeakPtrFactory<DataDevice> drop_weak_factory_{this};
  base::WeakPtrFactory<DataDevice> weak_factory_{this};
};

}  // namespace exo

#endif  // COMPONENTS_EXO_DATA_DEVICE_H_
