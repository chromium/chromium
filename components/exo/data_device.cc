// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/data_device.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "components/exo/data_device_delegate.h"
#include "components/exo/data_exchange_delegate.h"
#include "components/exo/data_offer.h"
#include "components/exo/data_source.h"
#include "components/exo/seat.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"

namespace exo {
namespace {

using ::ui::mojom::DragOperation;

constexpr int kDataDeviceSeatObserverPriority = 0;
static_assert(Seat::IsValidObserverPriority(kDataDeviceSeatObserverPriority),
              "kDataDeviceSeatObserverPriority is not in the valid range.");

constexpr base::TimeDelta kDataOfferDestructionTimeout =
    base::Milliseconds(1000);

DragOperation DndActionToDragOperation(DndAction dnd_action) {
  switch (dnd_action) {
    case DndAction::kMove:
      return DragOperation::kMove;
    case DndAction::kCopy:
      return DragOperation::kCopy;
    case DndAction::kAsk:
      return DragOperation::kLink;
    case DndAction::kNone:
      return DragOperation::kNone;
  }
}

}  // namespace

DataDevice::DataDevice(DataDeviceDelegate* delegate, Seat* seat)
    : delegate_(delegate), seat_(seat), drop_succeeded_(false) {
  ui::ClipboardMonitor::GetInstance()->AddObserver(this);

  seat_->AddObserver(this, kDataDeviceSeatObserverPriority);

  OnSurfaceFocused(seat_->GetFocusedSurface(), nullptr,
                   !!seat_->GetFocusedSurface());
}

DataDevice::~DataDevice() {
  delegate_->OnDataDeviceDestroying(this);

  ui::ClipboardMonitor::GetInstance()->RemoveObserver(this);

  seat_->RemoveObserver(this);
}

void DataDevice::StartDrag(DataSource* source,
                           Surface* origin,
                           Surface* icon,
                           ui::mojom::DragEventSource event_source) {
  seat_->StartDrag(source, origin, icon, event_source);
}

void DataDevice::SetSelection(DataSource* source) {
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

  data_offer_ =
      std::make_unique<ScopedDataOffer>(delegate_->OnDataOffer(), this);
  data_offer_->get()->SetDropData(seat_->data_exchange_delegate(),
                                  surface->window(), event.data());
  data_offer_->get()->SetSourceActions(dnd_actions);
  data_offer_->get()->SetActions(base::flat_set<DndAction>(), DndAction::kAsk);
  delegate_->OnEnter(surface, event.location_f(), *data_offer_->get());
}

aura::client::DragUpdateInfo DataDevice::OnDragUpdated(
    const ui::DropTargetEvent& event) {
  if (!data_offer_)
    return aura::client::DragUpdateInfo();

  ui::EndpointType endpoint_type = ui::EndpointType::kDefault;
  Surface* surface = GetEffectiveTargetForEvent(event);
  if (surface) {
    endpoint_type =
        seat_->data_exchange_delegate()->GetDataTransferEndpointType(
            surface->window());
  }
  aura::client::DragUpdateInfo drag_info(
      ui::DragDropTypes::DRAG_NONE, ui::DataTransferEndpoint(endpoint_type));

  delegate_->OnMotion(event.time_stamp(), event.location_f());

  // TODO(hirono): dnd_action() here may not be updated. Chrome needs to provide
  // a way to update DND action asynchronously.
  drag_info.drag_operation = static_cast<int>(
      DndActionToDragOperation(data_offer_->get()->dnd_action()));
  return drag_info;
}

void DataDevice::OnDragExited() {
  if (!data_offer_)
    return;

  delegate_->OnLeave();
  data_offer_.reset();
}

aura::client::DragDropDelegate::DropCallback DataDevice::GetDropCallback(
    const ui::DropTargetEvent& event) {
  base::ScopedClosureRunner drag_exit(
      base::BindOnce(&DataDevice::OnDragExited, weak_factory_.GetWeakPtr()));
  return base::BindOnce(&DataDevice::PerformDropOrExitDrag,
                        drop_weak_factory_.GetWeakPtr(), std::move(drag_exit));
}

void DataDevice::OnClipboardDataChanged() {
  if (!focused_surface_)
    return;
  SetSelectionToCurrentClipboardData();
}

void DataDevice::OnSurfaceCreated(Surface* surface) {
  if (delegate_->CanAcceptDataEventsForSurface(surface)) {
    aura::client::SetDragDropDelegate(surface->window(), this);
  }
}

void DataDevice::OnSurfaceFocused(Surface* gained_surface,
                                  Surface* lost_focused,
                                  bool has_focused_surface) {
  Surface* next_focused_surface =
      gained_surface && delegate_->CanAcceptDataEventsForSurface(gained_surface)
          ? gained_surface
          : nullptr;
  // Check if focused surface is not changed.
  if ((focused_surface_ && focused_surface_->get() == next_focused_surface) ||
      (!focused_surface_ && !next_focused_surface)) {
    return;
  }

  std::unique_ptr<ScopedSurface> last_focused_surface =
      std::move(focused_surface_);

  focused_surface_ = next_focused_surface ? std::make_unique<ScopedSurface>(
                                                next_focused_surface, this)
                                          : nullptr;
  // Check if the client newly obtained focus.
  if (focused_surface_ && !last_focused_surface)
    SetSelectionToCurrentClipboardData();
}

void DataDevice::OnDataOfferDestroying(DataOffer* data_offer) {
  if (data_offer_ && data_offer_->get() == data_offer) {
    drop_succeeded_ = data_offer_->get()->finished();
    if (quit_closure_)
      std::move(quit_closure_).Run();
    data_offer_.reset();
  }
  drop_weak_factory_.InvalidateWeakPtrs();
}

void DataDevice::OnSurfaceDestroying(Surface* surface) {
  if (focused_surface_ && focused_surface_->get() == surface) {
    DCHECK(surface->window());
    if (surface->window()) {
      aura::client::SetDragDropDelegate(surface->window(), nullptr);
    }
    focused_surface_.reset();
  }
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
  DCHECK(focused_surface_);
  DataOffer* data_offer = delegate_->OnDataOffer();
  data_offer->SetClipboardData(
      seat_->data_exchange_delegate(), *ui::Clipboard::GetForCurrentThread(),
      seat_->data_exchange_delegate()->GetDataTransferEndpointType(
          focused_surface_->get()->window()));
  delegate_->OnSelection(*data_offer);
}

void DataDevice::PerformDropOrExitDrag(
    base::ScopedClosureRunner exit_drag,
    std::unique_ptr<ui::OSExchangeData> data,
    ui::mojom::DragOperation& output_drag_op,
    std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
  exit_drag.ReplaceClosure(base::DoNothing());

  if (!data_offer_) {
    output_drag_op = DragOperation::kNone;
    return;
  }

  DndAction dnd_action = data_offer_->get()->dnd_action();

  delegate_->OnDrop();

  // TODO(crbug.com/40162278): Avoid using nested loop by adding asynchronous
  // callback to aura::client::DragDropDelegate.
  base::WeakPtr<DataDevice> alive(weak_factory_.GetWeakPtr());
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), kDataOfferDestructionTimeout);
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();

  if (!alive) {
    output_drag_op = DragOperation::kNone;
    return;
  }

  if (quit_closure_) {
    // DataOffer not destroyed by the client until the timeout.
    quit_closure_.Reset();
    data_offer_.reset();
    drop_succeeded_ = false;
  }

  if (!drop_succeeded_)
    output_drag_op = DragOperation::kNone;
  else
    output_drag_op = DndActionToDragOperation(dnd_action);
}

}  // namespace exo
