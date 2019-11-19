// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_DATA_DEVICE_H_
#define COMPONENTS_EXO_DATA_DEVICE_H_

#include <cstdint>

#include "base/macros.h"
#include "components/exo/data_offer_observer.h"
#include "components/exo/seat_observer.h"
#include "components/exo/surface.h"
#include "components/exo/surface_observer.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/base/dragdrop/drag_drop_types.h"

namespace ui {
class DropTargetEvent;
}

namespace exo {

class DataDeviceDelegate;
class DataOffer;
class ScopedDataOffer;
class DataSource;
class FileHelper;
class Seat;
class ScopedSurface;

enum class DndAction { kNone, kCopy, kMove, kAsk };

// DataDevice to start drag and drop and copy and paste oprations.
class DataDevice : public WMHelper::DragDropObserver,
                   public DataOfferObserver,
                   public ui::ClipboardObserver,
                   public SurfaceObserver,
                   public SeatObserver {
 public:
  explicit DataDevice(DataDeviceDelegate* delegate,
                      Seat* seat,
                      FileHelper* file_helper);
  ~DataDevice() override;

  // Starts drag-and-drop operation.
  // |source| represents data comes from the client starting drag operation. Can
  // be null if the data will be transferred only in the client.  |origin| is
  // the surface which starts the drag and drop operation. |icon| is the
  // nullable image which is rendered at the next to cursor while drag
  // operation. |serial| is the unique number comes from input events which
  // triggers the drag and drop operation.
  void StartDrag(DataSource* source,
                 Surface* origin,
                 Surface* icon,
                 ui::DragDropTypes::DragEventSource event_source);

  // Sets selection data to the clipboard.
  // |source| represents data comes from the client. |serial| is the unique
  // number comes from input events which triggers the drag and drop operation.
  void SetSelection(DataSource* source, uint32_t serial);

  // Overridden from WMHelper::DragDropObserver:
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  int OnPerformDrop(const ui::DropTargetEvent& event) override;

  // Overridden from ui::ClipbaordObserver:
  void OnClipboardDataChanged() override;

  // Overridden from SeatObserver:
  void OnSurfaceFocusing(Surface* surface) override;
  void OnSurfaceFocused(Surface* surface) override;

  // Overridden from DataOfferObserver:
  void OnDataOfferDestroying(DataOffer* data_offer) override;

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override;

  DataDeviceDelegate* get_delegate() { return delegate_; }

 private:
  Surface* GetEffectiveTargetForEvent(const ui::DropTargetEvent& event) const;
  void SetSelectionToCurrentClipboardData();

  DataDeviceDelegate* const delegate_;
  Seat* const seat_;
  FileHelper* const file_helper_;
  std::unique_ptr<ScopedDataOffer> data_offer_;
  std::unique_ptr<ScopedSurface> focused_surface_;

  DISALLOW_COPY_AND_ASSIGN(DataDevice);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_DATA_DEVICE_H_
