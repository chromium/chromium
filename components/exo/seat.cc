// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/seat.h"

#include <linux/input-event-codes.h>
#include <memory>
#include <variant>

#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "base/barrier_closure.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/exo/data_exchange_delegate.h"
#include "components/exo/data_source.h"
#include "components/exo/drag_drop_operation.h"
#include "components/exo/mime_utils.h"
#include "components/exo/seat_observer.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "components/exo/xkb_tracker.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/env.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint_serializer.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/ozone/evdev/mouse_button_property.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform_event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"

namespace exo {

namespace {

using CustomizableButton = ash::mojom::CustomizableButton;

std::optional<CustomizableButton> GetMouseButtonFromNativeEvent(
    const ui::PlatformEvent& platform_event) {
  auto event = ui::EventFromNative(platform_event);
  if (!event->IsMouseEvent() ||
      (event->type() != ui::EventType::kMouseReleased &&
       event->type() != ui::EventType::kMousePressed)) {
    return std::nullopt;
  }

  auto& mouse_event = *event->AsMouseEvent();
  switch (mouse_event.changed_button_flags()) {
    case ui::EF_LEFT_MOUSE_BUTTON:
      return CustomizableButton::kLeft;
    case ui::EF_RIGHT_MOUSE_BUTTON:
      return CustomizableButton::kRight;
    case ui::EF_MIDDLE_MOUSE_BUTTON:
      return CustomizableButton::kMiddle;
    case ui::EF_FORWARD_MOUSE_BUTTON:
    case ui::EF_BACK_MOUSE_BUTTON:
      break;
  }

  CHECK(mouse_event.changed_button_flags() == ui::EF_FORWARD_MOUSE_BUTTON ||
        mouse_event.changed_button_flags() == ui::EF_BACK_MOUSE_BUTTON);
  auto key_code = ui::GetForwardBackMouseButtonProperty(mouse_event);
  if (!key_code) {
    return (mouse_event.changed_button_flags() == ui::EF_FORWARD_MOUSE_BUTTON)
               ? CustomizableButton::kForward
               : CustomizableButton::kBack;
  }

  switch (*key_code) {
    case BTN_FORWARD:
      return CustomizableButton::kForward;
    case BTN_BACK:
      return CustomizableButton::kBack;
    case BTN_SIDE:
      return CustomizableButton::kSide;
    case BTN_EXTRA:
      return CustomizableButton::kExtra;
  }

  NOTREACHED();
}

bool IsPhysicalCodeEmpty(const PhysicalCode& code) {
  const auto* keyboard_physical_code = std::get_if<ui::DomCode>(&code);
  return keyboard_physical_code && *keyboard_physical_code == ui::DomCode::NONE;
}

}  // namespace

Seat::Seat(std::unique_ptr<DataExchangeDelegate> delegate)
    : changing_clipboard_data_to_selection_source_(false),
      data_exchange_delegate_(std::move(delegate)) {
  WMHelper::GetInstance()->AddFocusObserver(this);
  // Prepend handler as it's critical that we see all events.
  WMHelper::GetInstance()->PrependPreTargetHandler(this);
  ui::ClipboardMonitor::GetInstance()->AddObserver(this);
  // TODO(reveman): Need to handle the mus case where PlatformEventSource is
  // null. https://crbug.com/856230
  if (ui::PlatformEventSource::GetInstance())
    ui::PlatformEventSource::GetInstance()->AddPlatformEventObserver(this);

  ui_lock_controller_ = std::make_unique<UILockController>(this);

  // Seat needs to be registered as observers before any Keyboard,
  // because Keyboard expects that the XkbTracker is up-to-date when its
  // observer method is called.
  xkb_tracker_ = std::make_unique<XkbTracker>();
  ash::ImeControllerImpl* ime_controller = ash::Shell::Get()->ime_controller();
  xkb_tracker_->UpdateKeyboardLayout(ime_controller->keyboard_layout_name());
  ime_controller->AddObserver(this);
}

Seat::Seat() : Seat(nullptr) {}

Seat::~Seat() {
  Shutdown();
}

void Seat::Shutdown() {
  if (was_shutdown_)
    return;
  was_shutdown_ = true;
  DCHECK(!selection_source_) << "DataSource must be released before Seat";

  ash::Shell::Get()->ime_controller()->RemoveObserver(this);
  WMHelper::GetInstance()->RemoveFocusObserver(this);
  WMHelper::GetInstance()->RemovePreTargetHandler(this);
  ui::ClipboardMonitor::GetInstance()->RemoveObserver(this);
  if (ui::PlatformEventSource::GetInstance())
    ui::PlatformEventSource::GetInstance()->RemovePlatformEventObserver(this);
}

void Seat::AddObserver(SeatObserver* observer, int priority) {
  for (const auto& observer_list : priority_observer_list_)
    if (observer_list.HasObserver(observer))
      return;

  DCHECK(IsValidObserverPriority(priority));
  priority_observer_list_[priority].AddObserver(observer);
}

void Seat::RemoveObserver(SeatObserver* observer) {
  // We assume that the number of priority variations is small enough.
  for (auto& observer_list : priority_observer_list_)
    observer_list.RemoveObserver(observer);
}

void Seat::NotifySurfaceCreated(Surface* surface) {
  for (auto& observer_list : priority_observer_list_) {
    for (auto& observer : observer_list) {
      observer.OnSurfaceCreated(surface);
    }
  }
}

void Seat::NotifyPointerCaptureEnabled(Pointer* pointer,
                                       aura::Window* capture_window) {
  for (auto& observer_list : priority_observer_list_) {
    for (auto& observer : observer_list)
      observer.OnPointerCaptureEnabled(pointer, capture_window);
  }
}

void Seat::NotifyPointerCaptureDisabled(Pointer* pointer,
                                        aura::Window* capture_window) {
  for (auto& observer_list : priority_observer_list_) {
    for (auto& observer : observer_list)
      observer.OnPointerCaptureDisabled(pointer, capture_window);
  }
}

Surface* Seat::GetFocusedSurface() {
  return GetTargetSurfaceForKeyboardFocus(
      WMHelper::GetInstance()->GetFocusedWindow());
}

void Seat::StartDrag(DataSource* source,
                     Surface* origin,
                     Surface* icon,
                     ui::mojom::DragEventSource event_source) {
  gfx::Point cursor_location = aura::Env::GetInstance()->GetLastPointerPoint(
      event_source, origin->window(), /*fallback=*/std::nullopt);
  // DragDropOperation manages its own lifetime.
  drag_drop_operation_ = DragDropOperation::Create(
      data_exchange_delegate_.get(), source, origin, icon,
      gfx::PointF(cursor_location), event_source);
}

void Seat::AbortPendingDragOperation() {
  if (drag_drop_operation_)
    drag_drop_operation_->AbortIfPending();
}

bool Seat::IsDragDropOperationInProgress() const {
  return drag_drop_operation_ && drag_drop_operation_->started();
}

void Seat::SetSelection(DataSource* source) {
  Surface* focused_surface = GetFocusedSurface();
  if (!source || !focused_surface ||
      !source->CanBeDataSourceForCopy(focused_surface)) {
    if (source)
      source->Cancelled();
    return;
  }

  if (selection_source_) {
    if (selection_source_->get() == source)
      return;
    selection_source_->get()->Cancelled();
  }
  selection_source_ = std::make_unique<ScopedDataSource>(source, this);
  ui::EndpointType endpoint_type =
      data_exchange_delegate_->GetDataTransferEndpointType(
          focused_surface->window());
  scoped_refptr<RefCountedScopedClipboardWriter> writer =
      base::MakeRefCounted<RefCountedScopedClipboardWriter>(endpoint_type);

  size_t num_data_read_callbacks = DataSource::kMaxDataTypes;

  // Lacros sends additional metadata, in a custom MIME type, to sync clipboard
  // source metadata,
  if (endpoint_type == ui::EndpointType::kLacros)
    ++num_data_read_callbacks;

  base::RepeatingClosure data_read_callback = base::BarrierClosure(
      num_data_read_callbacks,
      base::BindOnce(&Seat::OnAllReadsFinished, weak_ptr_factory_.GetWeakPtr(),
                     writer));

  if (endpoint_type == ui::EndpointType::kLacros) {
    source->ReadDataTransferEndpoint(
        base::BindOnce(&Seat::OnDataTransferEndpointRead,
                       weak_ptr_factory_.GetWeakPtr(), writer,
                       data_read_callback),
        data_read_callback);
  }

  source->GetDataForPreferredMimeTypes(
      base::BindOnce(&Seat::OnTextRead, weak_ptr_factory_.GetWeakPtr(), writer,
                     data_read_callback),
      base::BindOnce(&Seat::OnRTFRead, weak_ptr_factory_.GetWeakPtr(), writer,
                     data_read_callback),
      base::BindOnce(&Seat::OnHTMLRead, weak_ptr_factory_.GetWeakPtr(), writer,
                     data_read_callback),
      base::BindOnce(&Seat::OnImageRead, weak_ptr_factory_.GetWeakPtr(), writer,
                     data_read_callback),
      base::BindOnce(&Seat::OnFilenamesRead, weak_ptr_factory_.GetWeakPtr(),
                     endpoint_type, writer, data_read_callback),
      DataSource::ReadFileContentsDataCallback(),
      base::BindOnce(&Seat::OnWebCustomDataRead, weak_ptr_factory_.GetWeakPtr(),
                     writer, data_read_callback),
      data_read_callback);
}

class Seat::RefCountedScopedClipboardWriter
    : public ui::ScopedClipboardWriter,
      public base::RefCounted<RefCountedScopedClipboardWriter> {
 public:
  explicit RefCountedScopedClipboardWriter(ui::EndpointType type)
      : ScopedClipboardWriter(
            ui::ClipboardBuffer::kCopyPaste,
            std::make_unique<ui::DataTransferEndpoint>(type)) {}

 private:
  friend class base::RefCounted<RefCountedScopedClipboardWriter>;
  virtual ~RefCountedScopedClipboardWriter() = default;
};

void Seat::OnDataTransferEndpointRead(
    scoped_refptr<RefCountedScopedClipboardWriter> writer,
    base::OnceClosure callback,
    const std::string& mime_type,
    std::u16string data) {
  std::string utf8_json = base::UTF16ToUTF8(data);
  auto clipboard_source = ui::ConvertJsonToDataTransferEndpoint(utf8_json);

  writer->SetDataSource(std::move(clipboard_source));
  std::move(callback).Run();
}

void Seat::OnTextRead(scoped_refptr<RefCountedScopedClipboardWriter> writer,
                      base::OnceClosure callback,
                      const std::string& mime_type,
                      std::u16string data) {
  writer->WriteText(std::move(data));
  std::move(callback).Run();
}

void Seat::OnRTFRead(scoped_refptr<RefCountedScopedClipboardWriter> writer,
                     base::OnceClosure callback,
                     const std::string& mime_type,
                     const std::vector<uint8_t>& data) {
  writer->WriteRTF(
      std::string(reinterpret_cast<const char*>(data.data()), data.size()));
  std::move(callback).Run();
}

void Seat::OnHTMLRead(scoped_refptr<RefCountedScopedClipboardWriter> writer,
                      base::OnceClosure callback,
                      const std::string& mime_type,
                      std::u16string data) {
  writer->WriteHTML(std::move(data), std::string());
  std::move(callback).Run();
}

void Seat::OnImageRead(scoped_refptr<RefCountedScopedClipboardWriter> writer,
                       base::OnceClosure callback,
                       const std::string& mime_type,
                       const std::vector<uint8_t>& data) {
  data_decoder::DecodeImageIsolated(
      data, data_decoder::mojom::ImageCodec::kDefault, false,
      std::numeric_limits<int64_t>::max(), gfx::Size(),
      base::BindOnce(&Seat::OnImageDecoded, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback), writer));
}

void Seat::OnImageDecoded(base::OnceClosure callback,
                          scoped_refptr<RefCountedScopedClipboardWriter> writer,
                          const SkBitmap& bitmap) {
  if (!bitmap.isNull() && !bitmap.empty())
    writer->WriteImage(bitmap);
  std::move(callback).Run();
}

void Seat::OnFilenamesRead(
    ui::EndpointType source,
    scoped_refptr<RefCountedScopedClipboardWriter> writer,
    base::OnceClosure callback,
    const std::string& mime_type,
    const std::vector<uint8_t>& data) {
  if (selection_source_) {
    std::vector<ui::FileInfo> filenames =
        selection_source_->get()->GetFilenames(source, data);
    writer->WriteFilenames(ui::FileInfosToURIList(filenames));
  }
  std::move(callback).Run();
}

void Seat::OnWebCustomDataRead(
    scoped_refptr<RefCountedScopedClipboardWriter> writer,
    base::OnceClosure callback,
    const std::string& mime_type,
    const std::vector<uint8_t>& data) {
  base::Pickle pickle = base::Pickle::WithUnownedBuffer(data);
  writer->WritePickledData(pickle,
                           ui::ClipboardFormatType::DataTransferCustomType());
  std::move(callback).Run();
}

void Seat::OnAllReadsFinished(
    scoped_refptr<RefCountedScopedClipboardWriter> writer) {
  // We need to destroy the ScopedClipboardWriter in this call, before
  // |auto_reset| is destroyed, so if there are outstanding references that
  // would prevent that, reschedule this task.
  if (!writer->HasOneRef()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&Seat::OnAllReadsFinished,
                       weak_ptr_factory_.GetWeakPtr(), std::move(writer)));
    return;
  }
  base::AutoReset<bool> auto_reset(
      &changing_clipboard_data_to_selection_source_, true);
  writer.reset();
}

////////////////////////////////////////////////////////////////////////////////
// aura::client::FocusChangeObserver overrides:

void Seat::OnWindowFocused(aura::Window* gained_focus,
                           aura::Window* lost_focus) {
  Surface* const gaining_focus_surface =
      GetTargetSurfaceForKeyboardFocus(gained_focus);
  Surface* const lost_focus_surface =
      GetTargetSurfaceForKeyboardFocus(lost_focus);

  for (auto& observer_list : priority_observer_list_) {
    for (auto& observer : observer_list)
      observer.OnSurfaceFocused(gaining_focus_surface, lost_focus_surface,
                                !!gained_focus);
  }
}

////////////////////////////////////////////////////////////////////////////////
// ui::PlatformEventObserver overrides:

void Seat::WillProcessEvent(const ui::PlatformEvent& event) {
  switch (ui::EventTypeFromNative(event)) {
    case ui::EventType::kKeyPressed:
    case ui::EventType::kKeyReleased:
      physical_code_for_currently_processing_event_ = ui::CodeFromNative(event);
      break;
    case ui::EventType::kMousePressed:
    case ui::EventType::kMouseReleased:
      if (auto button = GetMouseButtonFromNativeEvent(event); button) {
        physical_code_for_currently_processing_event_ = *button;
      }
      break;
    default:
      break;
  }
}

void Seat::DidProcessEvent(const ui::PlatformEvent& event) {
  switch (ui::EventTypeFromNative(event)) {
    case ui::EventType::kKeyPressed:
    case ui::EventType::kMousePressed:
      physical_code_for_currently_processing_event_ = ui::DomCode::NONE;
      break;
    case ui::EventType::kKeyReleased:
    case ui::EventType::kMouseReleased: {
      // Remove this from the pressed key map because when IME is active we can
      // end up getting the DidProcessEvent call before we get the OnKeyEvent
      // callback and then the key will end up being stuck pressed.
      if (!IsPhysicalCodeEmpty(physical_code_for_currently_processing_event_)) {
        pressed_keys_.erase(physical_code_for_currently_processing_event_);
        physical_code_for_currently_processing_event_ = ui::DomCode::NONE;
      }
    } break;
    default:
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// ui::EventHandler overrides:

void Seat::OnKeyEvent(ui::KeyEvent* event) {
  // Ignore synthetic key repeat events.
  if (event->is_repeat())
    return;

  if (!IsPhysicalCodeEmpty(physical_code_for_currently_processing_event_)) {
    switch (event->type()) {
      case ui::EventType::kKeyPressed: {
        auto& key_state_set =
            pressed_keys_[physical_code_for_currently_processing_event_];
        // Do not insert the additional events unless the event is a customized
        // button.
        if (!key_state_set.empty() &&
            !(event->flags() & ui::EF_IS_CUSTOMIZED_FROM_BUTTON)) {
          break;
        }
        key_state_set.emplace(event->code(), /*consumed_by_ime=*/false,
                              event->key_code());
      } break;
      case ui::EventType::kKeyReleased:
        pressed_keys_.erase(physical_code_for_currently_processing_event_);
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

  xkb_tracker_->UpdateKeyboardModifiers(event->flags());
  for (auto& observer_list : priority_observer_list_) {
    for (auto& observer : observer_list)
      observer.OnKeyboardModifierUpdated();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ui::ClipboardObserver overrides:

void Seat::OnClipboardDataChanged() {
  if (!selection_source_ || changing_clipboard_data_to_selection_source_)
    return;
  selection_source_->get()->Cancelled();
  selection_source_.reset();
}

UILockController* Seat::GetUILockControllerForTesting() {
  return ui_lock_controller_.get();
}

////////////////////////////////////////////////////////////////////////////////
// ash::ImeController::Observer overrides:

void Seat::OnCapsLockChanged(bool enabled) {}

void Seat::OnKeyboardLayoutNameChanged(const std::string& layout_name) {
  xkb_tracker_->UpdateKeyboardLayout(layout_name);
}

////////////////////////////////////////////////////////////////////////////////
// DataSourceObserver overrides:

void Seat::OnDataSourceDestroying(DataSource* source) {
  if (selection_source_ && selection_source_->get() == source)
    selection_source_.reset();
}

}  // namespace exo
