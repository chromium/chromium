// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/data_device.h"

#include "components/exo/data_device_delegate.h"
#include "components/exo/data_offer.h"
#include "components/exo/data_source.h"
#include "components/exo/seat.h"
#include "components/exo/surface.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"

namespace exo {

DataDevice::DataDevice(DataDeviceDelegate* delegate,
                       Seat* seat,
                       FileHelper* file_helper)
    : delegate_(delegate), seat_(seat), file_helper_(file_helper) {
  WMHelper::GetInstance()->AddDragDropObserver(this);
  ui::ClipboardMonitor::GetInstance()->AddObserver(this);

  seat_->AddObserver(this);

  OnSurfaceFocusing(seat_->GetFocusedSurface());
}

DataDevice::~DataDevice() {
  delegate_->OnDataDeviceDestroying(this);

  WMHelper::GetInstance()->RemoveDragDropObserver(this);
  ui::ClipboardMonitor::GetInstance()->RemoveObserver(this);

  seat_->RemoveObserver(this);
}

void DataDevice::StartDrag(DataSource* source,
                           Surface* origin,
                           Surface* icon,
                           ui::DragDropTypes::DragEventSource event_source) {
  seat_->StartDrag(source, origin, icon, event_source);
}

void DataDevice::SetSelection(DataSource* source, uint32_t serial) {
  // TODO(hirono): Check if serial is valid. crbug.com/746111
  seat_->SetSelection(source);
}

void DataDevice::OnDragEntered(const ui::DropTargetEvent& event) {
  DCHECK(!data_offer_);

  Surface* surface = GetEffectiveTargetForEvent(event);
  if (!surface)
    return;

  base::flat_set<DndAction> dnd_actions;
  if (event.source_operations() & ui::DragDropTypes::DRAG_MOVE) {
    dnd_actions.insert(DndAction::kMove);
  }
  if (event.source_operations() & ui::DragDropTypes::DRAG_COPY) {
    dnd_actions.insert(DndAction::kCopy);
  }
  if (event.source_operations() & ui::DragDropTypes::DRAG_LINK) {
    dnd_actions.insert(DndAction::kAsk);
  }

  data_offer_ = std::make_unique<ScopedDataOffer>(
      delegate_->OnDataOffer(DataOffer::DRAG_DROP), this);
  data_offer_->get()->SetDropData(file_helper_, event.data());
  data_offer_->get()->SetSourceActions(dnd_actions);
  data_offer_->get()->SetActions(base::flat_set<DndAction>(), DndAction::kAsk);
  delegate_->OnEnter(surface, event.location_f(), *data_offer_->get());
}

int DataDevice::OnDragUpdated(const ui::DropTargetEvent& event) {
  if (!data_offer_)
    return ui::DragDropTypes::DRAG_NONE;

  delegate_->OnMotion(event.time_stamp(), event.location_f());

  // TODO(hirono): dnd_action() here may not be updated. Chrome needs to provide
  // a way to update DND action asynchronously.
  switch (data_offer_->get()->dnd_action()) {
    case DndAction::kMove:
      return ui::DragDropTypes::DRAG_MOVE;
    case DndAction::kCopy:
      return ui::DragDropTypes::DRAG_COPY;
    case DndAction::kAsk:
      return ui::DragDropTypes::DRAG_LINK;
    case DndAction::kNone:
      return ui::DragDropTypes::DRAG_NONE;
  }
}

void DataDevice::OnDragExited() {
  if (!data_offer_)
    return;

  delegate_->OnLeave();
  data_offer_.reset();
}

int DataDevice::OnPerformDrop(const ui::DropTargetEvent& event) {
  if (!data_offer_)
    return ui::DragDropTypes::DRAG_NONE;

  delegate_->OnDrop();
  data_offer_.reset();
  return ui::DragDropTypes::DRAG_NONE;
}

void DataDevice::OnClipboardDataChanged() {
  if (!focused_surface_)
    return;
  SetSelectionToCurrentClipboardData();
}

void DataDevice::OnSurfaceFocusing(Surface* surface) {
  Surface* next_focused_surface =
      surface && delegate_->CanAcceptDataEventsForSurface(surface) ? surface
                                                                   : nullptr;
  // Check if focused surface is not changed.
  if (focused_surface_ && focused_surface_->get() == next_focused_surface)
    return;

  std::unique_ptr<ScopedSurface> last_focused_surface =
      std::move(focused_surface_);
  focused_surface_ = next_focused_surface ? std::make_unique<ScopedSurface>(
                                                next_focused_surface, this)
                                          : nullptr;

  // Check if the client newly obtained focus.
  if (focused_surface_ && !last_focused_surface)
    SetSelectionToCurrentClipboardData();
}

void DataDevice::OnSurfaceFocused(Surface* surface) {}

void DataDevice::OnDataOfferDestroying(DataOffer* data_offer) {
  if (data_offer_ && data_offer_->get() == data_offer)
    data_offer_.reset();
}

void DataDevice::OnSurfaceDestroying(Surface* surface) {
  if (focused_surface_ && focused_surface_->get() == surface)
    focused_surface_.reset();
}

Surface* DataDevice::GetEffectiveTargetForEvent(
    const ui::DropTargetEvent& event) const {
  aura::Window* window = static_cast<aura::Window*>(event.target());
  if (!window)
    return nullptr;
  Surface* target = Surface::AsSurface(window);
  if (!target)
    return nullptr;

  return delegate_->CanAcceptDataEventsForSurface(target) ? target : nullptr;
}

void DataDevice::SetSelectionToCurrentClipboardData() {
  DataOffer* data_offer = delegate_->OnDataOffer(DataOffer::COPY_PASTE);
  data_offer->SetClipboardData(file_helper_,
                               *ui::Clipboard::GetForCurrentThread());
  delegate_->OnSelection(*data_offer);
}

}  // namespace exo
