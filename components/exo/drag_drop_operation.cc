// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/drag_drop_operation.h"

#include "ash/drag_drop/drag_drop_controller.h"
#include "base/barrier_closure.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/exo/data_offer.h"
#include "components/exo/data_source.h"
#include "components/exo/seat.h"
#include "components/exo/surface.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/transform_util.h"

namespace exo {

namespace {

uint32_t DndActionsToDragOperations(const base::flat_set<DndAction>& actions) {
  uint32_t dnd_operations = 0;
  for (const DndAction action : actions) {
    switch (action) {
      case DndAction::kNone:
        FALLTHROUGH;
        // We don't support the ask action
      case DndAction::kAsk:
        break;
      case DndAction::kCopy:
        dnd_operations |= ui::DragDropTypes::DragOperation::DRAG_COPY;
        break;
      case DndAction::kMove:
        dnd_operations |= ui::DragDropTypes::DragOperation::DRAG_MOVE;
        break;
    }
  }
  return dnd_operations;
}

#if defined(OS_CHROMEOS)
DndAction DragOperationsToPreferredDndAction(int op) {
  if (op & ui::DragDropTypes::DragOperation::DRAG_COPY)
    return DndAction::kCopy;

  if (op & ui::DragDropTypes::DragOperation::DRAG_MOVE)
    return DndAction::kMove;

  return DndAction::kNone;
}
#endif

DndAction DragOperationToDndAction(int op) {
  switch (op) {
    case ui::DragDropTypes::DragOperation::DRAG_NONE:
      return DndAction::kNone;
    case ui::DragDropTypes::DragOperation::DRAG_MOVE:
      return DndAction::kMove;
    case ui::DragDropTypes::DragOperation::DRAG_COPY:
      return DndAction::kCopy;
    default:
      NOTREACHED();
      return DndAction::kNone;
  }
}

}  // namespace

base::WeakPtr<DragDropOperation> DragDropOperation::Create(
    DataSource* source,
    Surface* origin,
    Surface* icon,
    ui::DragDropTypes::DragEventSource event_source) {
  auto* dnd_op = new DragDropOperation(source, origin, icon, event_source);
  return dnd_op->weak_ptr_factory_.GetWeakPtr();
}

DragDropOperation::DragDropOperation(
    DataSource* source,
    Surface* origin,
    Surface* icon,
    ui::DragDropTypes::DragEventSource event_source)
    : SurfaceTreeHost("ExoDragDropOperation"),
      source_(std::make_unique<ScopedDataSource>(source, this)),
      origin_(std::make_unique<ScopedSurface>(origin, this)),
      drag_start_point_(display::Screen::GetScreen()->GetCursorScreenPoint()),
      os_exchange_data_(std::make_unique<ui::OSExchangeData>()),
      event_source_(event_source),
      weak_ptr_factory_(this) {
  aura::Window* root_window = origin_->get()->window()->GetRootWindow();
  DCHECK(root_window);
#if defined(OS_CHROMEOS)
  drag_drop_controller_ = static_cast<ash::DragDropController*>(
      aura::client::GetDragDropClient(root_window));
#else
  drag_drop_controller_ = aura::client::GetDragDropClient(root_window);
#endif
  DCHECK(drag_drop_controller_);

  if (drag_drop_controller_->IsDragDropInProgress())
    drag_drop_controller_->DragCancel();

  drag_drop_controller_->AddObserver(this);

  if (icon)
    icon_ = std::make_unique<ScopedSurface>(icon, this);

  auto start_op_callback =
      base::BindOnce(&DragDropOperation::ScheduleStartDragDropOperation,
                     weak_ptr_factory_.GetWeakPtr());

  // Make the count kMaxClipboardDataTypes + 1 so we can wait for the icon to be
  // captured as well.
  counter_ = base::BarrierClosure(kMaxClipboardDataTypes + 1,
                                  std::move(start_op_callback));

  source->GetDataForPreferredMimeTypes(
      base::BindOnce(&DragDropOperation::OnTextRead,
                     weak_ptr_factory_.GetWeakPtr()),
      DataSource::ReadDataCallback(),
      base::BindOnce(&DragDropOperation::OnHTMLRead,
                     weak_ptr_factory_.GetWeakPtr()),
      DataSource::ReadDataCallback(), counter_);

  if (icon) {
    origin_->get()->window()->AddChild(host_window());
    SetRootSurface(icon);
  }
}

DragDropOperation::~DragDropOperation() {
  drag_drop_controller_->RemoveObserver(this);

  if (source_)
    source_->get()->Cancelled();

  if (drag_drop_controller_->IsDragDropInProgress() && started_by_this_object_)
    drag_drop_controller_->DragCancel();
}

void DragDropOperation::AbortIfPending() {
  if (!started_by_this_object_)
    delete this;
}

void DragDropOperation::OnTextRead(const std::string& mime_type,
                                   base::string16 data) {
  DCHECK(os_exchange_data_);
  os_exchange_data_->SetString(std::move(data));

  // Prefer to use the HTML MIME type if possible
  if (mime_type_.empty())
    mime_type_ = mime_type;
  counter_.Run();
}

void DragDropOperation::OnHTMLRead(const std::string& mime_type,
                                   base::string16 data) {
  DCHECK(os_exchange_data_);
  os_exchange_data_->SetHtml(std::move(data), GURL());
  mime_type_ = mime_type;
  counter_.Run();
}

void DragDropOperation::OnSurfaceCommit() {
  SurfaceTreeHost::OnSurfaceCommit();

  if (icon_)
    CaptureDragIcon();
}

void DragDropOperation::CaptureDragIcon() {
  SubmitCompositorFrame();

  std::unique_ptr<viz::CopyOutputRequest> request =
      std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA_BITMAP,
          base::BindOnce(&DragDropOperation::OnDragIconCaptured,
                         weak_ptr_factory_.GetWeakPtr()));

  host_window()->layer()->RequestCopyOfOutput(std::move(request));
}

void DragDropOperation::OnDragIconCaptured(
    std::unique_ptr<viz::CopyOutputResult> icon_result) {
  gfx::ImageSkia icon_skia;

  // An empty response means the request was deleted before it was completed. If
  // this happens, and no operation has yet finished, restart the capture.
  if (icon_result->IsEmpty()) {
    CaptureDragIcon();
    return;
  }

  float scale_factor = origin_->get()->window()->layer()->device_scale_factor();
  icon_skia = gfx::ImageSkia(
      gfx::ImageSkiaRep(icon_result->AsSkBitmap(), scale_factor));

  if (os_exchange_data_) {
    os_exchange_data_->provider().SetDragImage(
        icon_skia, -icon_->get()->GetBufferOffset());
  } else {
#if defined(OS_CHROMEOS)
    drag_drop_controller_->SetDragImage(icon_skia,
                                        -icon_->get()->GetBufferOffset());
#endif
  }

  if (!captured_icon_) {
    captured_icon_ = true;
    counter_.Run();
  }
}

void DragDropOperation::ScheduleStartDragDropOperation() {
  // StartDragAndDrop uses a nested run loop. When restarting, we a) don't want
  // to interrupt the callers task for an arbitrary period of time and b) want
  // to let any nested run loops that are currently running to have a chance to
  // exit to avoid arbitrarily deep nesting. We can accomplish both of those
  // things by posting a new task to actually start the drag and drop operation.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&DragDropOperation::StartDragDropOperation,
                                weak_ptr_factory_.GetWeakPtr()));
}

void DragDropOperation::StartDragDropOperation() {
  uint32_t dnd_operations =
      DndActionsToDragOperations(source_->get()->GetActions());

  started_by_this_object_ = true;
  // This triggers a nested run loop that terminates when the drag and drop
  // operation is completed.
  int op = drag_drop_controller_->StartDragAndDrop(
      std::move(os_exchange_data_), origin_->get()->window()->GetRootWindow(),
      origin_->get()->window(), drag_start_point_, dnd_operations,
      event_source_);

  if (op) {
    // Success

    // TODO(crbug.com/994065) This is currently not the actual mime type used by
    // the recipient, just an arbitrary one we pick out of the offered types so
    // we can report back whether or not the drop can succeed. This may need to
    // change in the future.
    source_->get()->Target(mime_type_);

    source_->get()->Action(DragOperationToDndAction(op));
    source_->get()->DndDropPerformed();
    source_->get()->DndFinished();

    // Reset |source_| so it the destructor doesn't try to cancel it.
    source_.reset();
  }

  // On failure the destructor will handle canceling the data source.
  delete this;
}

void DragDropOperation::OnDragStarted() {
  if (!started_by_this_object_)
    delete this;
}

void DragDropOperation::OnDragEnded() {}

#if defined(OS_CHROMEOS)
void DragDropOperation::OnDragActionsChanged(int actions) {
  if (!started_by_this_object_)
    return;

  DndAction dnd_action = DragOperationsToPreferredDndAction(actions);
  // We send a mime type along with the action to indicate to the application
  // that dropping is/is not currently possible. We do not currently know of
  // any applications that care about the specific mime type until the drop is
  // actually performed.
  if (dnd_action != DndAction::kNone)
    source_->get()->Target(mime_type_);
  else
    source_->get()->Target(base::nullopt);

  source_->get()->Action(dnd_action);
}
#endif

void DragDropOperation::OnSurfaceDestroying(Surface* surface) {
  if (surface == origin_->get() || surface == icon_->get()) {
    delete this;
  } else {
    NOTREACHED();
  }
}

void DragDropOperation::OnDataSourceDestroying(DataSource* source) {
  if (source == source_->get()) {
    source_.reset();
    delete this;
  } else {
    NOTREACHED();
  }
}
}  // namespace exo
