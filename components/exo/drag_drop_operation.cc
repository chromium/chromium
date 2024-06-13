// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/drag_drop_operation.h"

#include "ash/drag_drop/drag_drop_controller.h"
#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/pickle.h"
#include "base/strings/string_split.h"
#include "base/task/sequenced_task_runner.h"
#include "components/exo/data_exchange_delegate.h"
#include "components/exo/data_offer.h"
#include "components/exo/data_source.h"
#include "components/exo/extended_drag_source.h"
#include "components/exo/seat.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/surface_tree_host.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint_serializer.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/geometry/vector2d.h"
#include "url/gurl.h"

namespace exo {
namespace {

using ::ui::mojom::DragOperation;

uint32_t DndActionsToDragOperations(const base::flat_set<DndAction>& actions) {
  uint32_t dnd_operations = 0;
  for (const DndAction action : actions) {
    switch (action) {
      case DndAction::kNone:
        [[fallthrough]];
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

DndAction DragOperationsToPreferredDndAction(int op) {
  if (op & ui::DragDropTypes::DragOperation::DRAG_COPY)
    return DndAction::kCopy;

  if (op & ui::DragDropTypes::DragOperation::DRAG_MOVE)
    return DndAction::kMove;

  return DndAction::kNone;
}

DndAction DragOperationToDndAction(DragOperation op) {
  switch (op) {
    case DragOperation::kNone:
      return DndAction::kNone;
    case DragOperation::kMove:
      return DndAction::kMove;
    case DragOperation::kCopy:
      return DndAction::kCopy;
    case DragOperation::kLink:
      return DndAction::kAsk;
    default:
      NOTREACHED_IN_MIGRATION() << op;
      return DndAction::kNone;
  }
  NOTREACHED_IN_MIGRATION();
}

}  // namespace

// Internal representation of a drag icon surface. Used when a non-null surface
// is passed in wl_data_device::start_drag requests.
// TODO(crbug.com/40145458): Rework icon implementation to avoid frame copies.
class DragDropOperation::IconSurface final : public SurfaceTreeHost,
                                             public ScopedSurface {
 public:
  IconSurface(Surface* icon, DragDropOperation* operation)
      : SurfaceTreeHost("ExoDragIcon"),
        ScopedSurface(icon, operation),
        operation_(operation) {
    DCHECK(operation_);
    DCHECK(!icon->HasSurfaceDelegate());

    Surface* origin_surface = operation_->origin_->get();
    origin_surface->window()->AddChild(host_window());
    SetRootSurface(icon);
  }

  IconSurface(const IconSurface&) = delete;
  IconSurface& operator=(const IconSurface&) = delete;
  ~IconSurface() override = default;

 private:
  // SurfaceTreeHost:
  void OnSurfaceCommit() override {
    SurfaceTreeHost::OnSurfaceCommit();
    RequestCaptureIcon();
  }

  void RequestCaptureIcon() {
    SubmitCompositorFrame();

    std::unique_ptr<viz::CopyOutputRequest> request =
        std::make_unique<viz::CopyOutputRequest>(
            viz::CopyOutputRequest::ResultFormat::RGBA,
            viz::CopyOutputRequest::ResultDestination::kSystemMemory,
            base::BindOnce(&IconSurface::OnCaptured,
                           weak_ptr_factory_.GetWeakPtr()));
    request->set_result_task_runner(
        base::SequencedTaskRunner::GetCurrentDefault());

    host_window()->layer()->RequestCopyOfOutput(std::move(request));
  }

  void OnCaptured(std::unique_ptr<viz::CopyOutputResult> icon_result) {
    // An empty response means the request was deleted before it was completed.
    // If this happens, and no operation has yet finished, restart the capture.
    if (icon_result->IsEmpty()) {
      RequestCaptureIcon();
      return;
    }

    auto scoped_bitmap = icon_result->ScopedAccessSkBitmap();
    operation_->OnDragIconCaptured(scoped_bitmap.GetOutScopedBitmap());
  }

  const raw_ptr<DragDropOperation> operation_;
  base::WeakPtrFactory<IconSurface> weak_ptr_factory_{this};
};

base::WeakPtr<DragDropOperation> DragDropOperation::Create(
    DataExchangeDelegate* data_exchange_delegate,
    DataSource* source,
    Surface* origin,
    Surface* icon,
    const gfx::PointF& drag_start_point,
    ui::mojom::DragEventSource event_source) {
  auto* dnd_op = new DragDropOperation(data_exchange_delegate, source, origin,
                                       icon, drag_start_point, event_source);
  return dnd_op->weak_ptr_factory_.GetWeakPtr();
}

DragDropOperation::DragDropOperation(
    DataExchangeDelegate* data_exchange_delegate,
    DataSource* source,
    Surface* origin,
    Surface* icon,
    const gfx::PointF& drag_start_point,
    ui::mojom::DragEventSource event_source)
    : source_(std::make_unique<ScopedDataSource>(source, this)),
      origin_(std::make_unique<ScopedSurface>(origin, this)),
      drag_start_point_(drag_start_point),
      os_exchange_data_(std::make_unique<ui::OSExchangeData>()),
      event_source_(event_source) {
  aura::Window* root_window = origin_->get()->window()->GetRootWindow();
  DCHECK(root_window);
  drag_drop_controller_ = static_cast<ash::DragDropController*>(
      aura::client::GetDragDropClient(root_window));
  DCHECK(drag_drop_controller_);

  if (drag_drop_controller_->IsDragDropInProgress())
    drag_drop_controller_->DragCancel();

  drag_drop_controller_->AddObserver(this);

  ui::EndpointType endpoint_type =
      data_exchange_delegate->GetDataTransferEndpointType(
          origin_->get()->window());
  os_exchange_data_->SetSource(
      std::make_unique<ui::DataTransferEndpoint>(endpoint_type));

  extended_drag_source_ = ExtendedDragSource::Get();
  if (extended_drag_source_) {
    drag_drop_controller_->set_toplevel_window_drag_delegate(
        extended_drag_source_);
    extended_drag_source_->AddObserver(this);
  }

  int num_additional_callbacks = 0;

  // TODO(crbug.com/40061238): Remove this once the issue is fixed.
  std::string callbacks;

  // TODO(crbug.com/1298033): Move DTE retrieval into
  // DataSource::GetDataForPreferredMimeTypes()
  // Lacros sends additional metadata, in a custom MIME type, to sync drag
  // source metadata. Hence, the number of callbacks is incremented by one.
  if (endpoint_type == ui::EndpointType::kLacros) {
    callbacks += "lacros,";
    ++num_additional_callbacks;
  }

  // When the icon is present, we increment the number of callbacks so we can
  // wait for the icon to be captured as well.
  if (icon) {
    icon_ = std::make_unique<IconSurface>(icon, this);
    ++num_additional_callbacks;
    callbacks += "icon,";
  }

  auto start_op_callback =
      base::BindOnce(&DragDropOperation::ScheduleStartDragDropOperation,
                     weak_ptr_factory_.GetWeakPtr());

  // TODO(crbug.com/40061238): Remove these when the issue is fixed.
  start_drag_drop_timer_.Start(FROM_HERE, base::Seconds(2), this,
                               &DragDropOperation::DragDataReadTimeout);
  LOG(ERROR) << "Starting data read for drag operation: additonal callbacks:"
             << callbacks;

  counter_ =
      base::BarrierClosure(DataSource::kMaxDataTypes + num_additional_callbacks,
                           std::move(start_op_callback));

  // TODO(crbug.com/1298033): Move DTE retrieval into
  // DataSource::GetDataForPreferredMimeTypes()
  if (endpoint_type == ui::EndpointType::kLacros) {
    source->ReadDataTransferEndpoint(
        base::BindOnce(&DragDropOperation::OnDataTransferEndpointRead,
                       weak_ptr_factory_.GetWeakPtr()),
        counter_);
  }

  source->GetDataForPreferredMimeTypes(
      base::BindOnce(&DragDropOperation::OnTextRead,
                     weak_ptr_factory_.GetWeakPtr()),
      DataSource::ReadDataCallback(),
      base::BindOnce(&DragDropOperation::OnHTMLRead,
                     weak_ptr_factory_.GetWeakPtr()),
      DataSource::ReadDataCallback(),
      base::BindOnce(&DragDropOperation::OnFilenamesRead,
                     weak_ptr_factory_.GetWeakPtr(), data_exchange_delegate,
                     origin->window()),
      base::BindOnce(&DragDropOperation::OnFileContentsRead,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&DragDropOperation::OnWebCustomDataRead,
                     weak_ptr_factory_.GetWeakPtr()),
      counter_);
}

DragDropOperation::~DragDropOperation() {
  drag_drop_controller_->RemoveObserver(this);

  if (source_)
    source_->get()->Cancelled();

  if (drag_drop_controller_->IsDragDropInProgress() && started_) {
    drag_drop_controller_->DragCancel();
  }

  if (extended_drag_source_)
    ResetExtendedDragSource();
}

void DragDropOperation::AbortIfPending() {
  if (!started_) {
    delete this;
  }
}

void DragDropOperation::OnDataTransferEndpointRead(const std::string& mime_type,
                                                   std::u16string data) {
  DCHECK(os_exchange_data_);

  std::string utf8_json = base::UTF16ToUTF8(data);
  auto drag_source_dte = ui::ConvertJsonToDataTransferEndpoint(utf8_json);

  os_exchange_data_->SetSource(std::move(drag_source_dte));

  counter_.Run();
}

void DragDropOperation::OnTextRead(const std::string& mime_type,
                                   std::u16string data) {
  DCHECK(os_exchange_data_);
  os_exchange_data_->SetString(std::move(data));

  // Prefer to use the HTML MIME type if possible
  if (mime_type_.empty())
    mime_type_ = mime_type;
  counter_.Run();
}

void DragDropOperation::OnHTMLRead(const std::string& mime_type,
                                   std::u16string data) {
  DCHECK(os_exchange_data_);
  os_exchange_data_->SetHtml(std::move(data), GURL());
  mime_type_ = mime_type;
  counter_.Run();
}

void DragDropOperation::OnFilenamesRead(
    DataExchangeDelegate* data_exchange_delegate,
    aura::Window* source,
    const std::string& mime_type,
    const std::vector<uint8_t>& data) {
  DCHECK(os_exchange_data_);
  os_exchange_data_->SetFilenames(source_->get()->GetFilenames(
      data_exchange_delegate->GetDataTransferEndpointType(source), data));
  mime_type_ = mime_type;
  counter_.Run();
}

void DragDropOperation::OnFileContentsRead(const std::string& mime_type,
                                           const base::FilePath& filename,
                                           const std::vector<uint8_t>& data) {
  DCHECK(os_exchange_data_);
  os_exchange_data_->SetFileContents(filename,
                                     std::string(data.begin(), data.end()));
  mime_type_ = mime_type;
  counter_.Run();
}

void DragDropOperation::OnWebCustomDataRead(const std::string& mime_type,
                                            const std::vector<uint8_t>& data) {
  DCHECK(os_exchange_data_);
  base::Pickle pickle = base::Pickle::WithUnownedBuffer(data);
  os_exchange_data_->SetPickledData(
      ui::ClipboardFormatType::DataTransferCustomType(), pickle);
  mime_type_ = mime_type;
  counter_.Run();
}

void DragDropOperation::OnDragIconCaptured(const SkBitmap& icon_bitmap) {
  DCHECK(icon_);

  float scale_factor = origin_->get()->window()->layer()->device_scale_factor();
  gfx::ImageSkia icon_skia =
      gfx::ImageSkia::CreateFromBitmap(icon_bitmap, scale_factor);
  gfx::Vector2d icon_offset = -icon_->get()->GetBufferOffset();

  if (os_exchange_data_) {
    os_exchange_data_->provider().SetDragImage(icon_skia, icon_offset);
  } else {
    drag_drop_controller_->SetDragImage(icon_skia, icon_offset);
  }

  if (!captured_icon_) {
    captured_icon_ = true;
    counter_.Run();
  }
}

void DragDropOperation::ScheduleStartDragDropOperation() {
  start_drag_drop_timer_.Stop();

  // StartDragAndDrop uses a nested run loop. When restarting, we a) don't want
  // to interrupt the callers task for an arbitrary period of time and b) want
  // to let any nested run loops that are currently running to have a chance to
  // exit to avoid arbitrarily deep nesting. We can accomplish both of those
  // things by posting a new task to actually start the drag and drop operation.
  if (extended_drag_source_) {
    ShellSurfaceBase* shell_surface = GetShellSurfaceBaseForWindow(
        origin_->get()->window()->GetToplevelWindow());
    if (shell_surface)
      shell_surface->set_in_extended_drag(true);
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&DragDropOperation::StartDragDropOperation,
                                weak_ptr_factory_.GetWeakPtr()));
}

void DragDropOperation::StartDragDropOperation() {
  uint32_t dnd_operations =
      DndActionsToDragOperations(source_->get()->GetActions());

  base::WeakPtr<DragDropOperation> weak_ptr = weak_ptr_factory_.GetWeakPtr();

  started_ = true;
  gfx::Point drag_start_point = gfx::ToFlooredPoint(drag_start_point_);

  // This triggers a nested run loop that terminates when the drag and drop
  // operation is completed.
  DragOperation op = drag_drop_controller_->StartDragAndDrop(
      std::move(os_exchange_data_), origin_->get()->window()->GetRootWindow(),
      origin_->get()->window(), drag_start_point, dnd_operations,
      event_source_);

  // The instance deleted during StartDragAndDrop's nested RunLoop.
  if (!weak_ptr)
    return;

  // Always reset the in_extended_drag becacuse ExtendedDragSource may be
  // destroyed during nested loop.
  if (origin_->get()) {
    ShellSurfaceBase* shell_surface = GetShellSurfaceBaseForWindow(
        origin_->get()->window()->GetToplevelWindow());
    if (shell_surface)
      shell_surface->set_in_extended_drag(false);
  }

  // In tests, drag_drop_controller_ does not create a nested message loop and
  // so StartDragAndDrop exits before the drag&drop session finishes. In that
  // case the cleanup process shouldn't be made.
  if (drag_drop_controller_->IsDragDropInProgress())
    return;

  if (op != DragOperation::kNone) {
    // Success

    // TODO(crbug.com/994065) This is currently not the actual mime type
    // used by the recipient, just an arbitrary one we pick out of the
    // offered types so we can report back whether or not the drop can
    // succeed. This may need to change in the future.
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
  if (!started_) {
    delete this;
  }
}

void DragDropOperation::OnDragActionsChanged(int actions) {
  if (!started_) {
    return;
  }

  DndAction dnd_action = DragOperationsToPreferredDndAction(actions);
  // We send a mime type along with the action to indicate to the application
  // that dropping is/is not currently possible. We do not currently know of
  // any applications that care about the specific mime type until the drop is
  // actually performed.
  if (dnd_action != DndAction::kNone)
    source_->get()->Target(mime_type_);
  else
    source_->get()->Target(std::nullopt);

  source_->get()->Action(dnd_action);
}

void DragDropOperation::OnExtendedDragSourceDestroying(
    ExtendedDragSource* source) {
  ResetExtendedDragSource();
}

void DragDropOperation::ResetExtendedDragSource() {
  DCHECK(extended_drag_source_);
  extended_drag_source_->RemoveObserver(this);
  drag_drop_controller_->set_toplevel_window_drag_delegate(nullptr);
  extended_drag_source_ = nullptr;
}

void DragDropOperation::OnSurfaceDestroying(Surface* surface) {
  DCHECK(surface == origin_->get() || (icon_ && surface == icon_->get()));
  delete this;
}

void DragDropOperation::OnDataSourceDestroying(DataSource* source) {
  DCHECK_EQ(source, source_->get());
  source_.reset();
  LOG(ERROR) << "DataSource was destroyed by client";
  delete this;
}

void DragDropOperation::DragDataReadTimeout() {
  LOG(ERROR) << "DragDataReadTimeout";
}

}  // namespace exo
