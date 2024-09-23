// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SEAT_H_
#define COMPONENTS_EXO_SEAT_H_

#include <array>

#include "ash/ime/ime_controller_impl.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/exo/data_source_observer.h"
#include "components/exo/key_state.h"
#include "components/exo/ui_lock_controller.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/events/event_handler.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/platform/platform_event_observer.h"

namespace ui {
class KeyEvent;
}  // namespace ui

namespace exo {
class DragDropOperation;
class DataExchangeDelegate;
class Pointer;
class ScopedDataSource;
class SeatObserver;
class Surface;
class XkbTracker;

// Seat object represent a group of input devices such as keyboard, pointer and
// touch devices and keeps track of input focus.
class Seat : public aura::client::FocusChangeObserver,
             public ui::PlatformEventObserver,
             public ui::EventHandler,
             public ui::ClipboardObserver,
             public ash::ImeController::Observer,
             public DataSourceObserver {
 public:
  explicit Seat(std::unique_ptr<DataExchangeDelegate> delegate);
  Seat();
  Seat(const Seat&) = delete;
  Seat& operator=(const Seat&) = delete;
  ~Seat() override;

  using FocusChangedCallback =
      base::RepeatingCallback<void(Surface*, Surface*, bool)>;

  void Shutdown();

  // Registers the observer with the given priority.
  // Observers with smaller priority value will be called earlier.
  // The same observer should not be registered twice or more even with
  // different priorities. If the order of observer invocations with
  // the same priority is implementation dependent.
  // The priority must be in a range of
  // [0, kMaxObserverPriority] inclusive.
  void AddObserver(SeatObserver* observer, int priority);

  // Unregisters the observer.
  void RemoveObserver(SeatObserver* observer);

  // Returns true if the given priority can be used for AddObserver.
  static constexpr bool IsValidObserverPriority(int priority) {
    return 0 <= priority && priority <= kMaxObserverPriority;
  }

  // Nontify observers when a new surface is created.
  void NotifySurfaceCreated(Surface* surface);

  // Notify observers about pointer capture state changes.
  void NotifyPointerCaptureEnabled(Pointer* pointer,
                                   aura::Window* capture_window);
  void NotifyPointerCaptureDisabled(Pointer* pointer,
                                    aura::Window* capture_window);

  // Returns currently focused surface. This is virtual so that we can override
  // the behavior for testing.
  virtual Surface* GetFocusedSurface();

  // Returns currently pressed keys.
  const base::flat_map<PhysicalCode, base::flat_set<KeyState>>& pressed_keys()
      const {
    return pressed_keys_;
  }

  const XkbTracker* xkb_tracker() const { return xkb_tracker_.get(); }

  DataExchangeDelegate* data_exchange_delegate() {
    return data_exchange_delegate_.get();
  }

  // Returns physical code for the currently processing event.
  const PhysicalCode& physical_code_for_currently_processing_event() const {
    return physical_code_for_currently_processing_event_;
  }

  // Sets clipboard data from |source|.
  void SetSelection(DataSource* source);

  void StartDrag(DataSource* source,
                 Surface* origin,
                 Surface* icon,
                 ui::mojom::DragEventSource event_source);

  // Abort any drag operations that haven't been started yet.
  void AbortPendingDragOperation();

  // Returns true if the drag and drop has been started (which happens
  // asynchronously) and hasn't been fully finished yet. This can return true
  // even if ash's DND session is finished because wayland's dnd finished event
  // is sent asynchronosly.
  bool IsDragDropOperationInProgress() const;

  // Overridden from aura::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // Overridden from ui::PlatformEventObserver:
  void WillProcessEvent(const ui::PlatformEvent& event) override;
  void DidProcessEvent(const ui::PlatformEvent& event) override;

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

  // Overridden from ui::ClipboardObserver:
  void OnClipboardDataChanged() override;

  // Overridden from DataSourceObserver:
  void OnDataSourceDestroying(DataSource* source) override;

  // Overridden from ash::ImeController::Observer:
  void OnCapsLockChanged(bool enabled) override;
  void OnKeyboardLayoutNameChanged(const std::string& layout_name) override;

  UILockController* GetUILockControllerForTesting();

  void set_physical_code_for_currently_processing_event_for_testing(
      PhysicalCode physical_code_for_currently_processing_event) {
    physical_code_for_currently_processing_event_ =
        physical_code_for_currently_processing_event;
  }

  base::WeakPtr<DragDropOperation> get_drag_drop_operation_for_testing() {
    return drag_drop_operation_;
  }

  bool was_shutdown() const { return was_shutdown_; }

 private:
  class RefCountedScopedClipboardWriter;

  // Called when the focused window is a Lacros window and a source
  // DataTransferEndpoint is read in the available MIME types. This
  // is currently used to synchronize clipboard source metadata from
  // Lacros to Ash.
  void OnDataTransferEndpointRead(
      scoped_refptr<RefCountedScopedClipboardWriter> writer,
      base::OnceClosure callback,
      const std::string& mime_type,
      std::u16string data);

  // Called when data is read from FD passed from a client.
  // |data| is read data. |source| is source of the data, or nullptr if
  // DataSource has already been destroyed.
  void OnTextRead(scoped_refptr<RefCountedScopedClipboardWriter> writer,
                  base::OnceClosure callback,
                  const std::string& mime_type,
                  std::u16string data);
  void OnRTFRead(scoped_refptr<RefCountedScopedClipboardWriter> writer,
                 base::OnceClosure callback,
                 const std::string& mime_type,
                 const std::vector<uint8_t>& data);
  void OnHTMLRead(scoped_refptr<RefCountedScopedClipboardWriter> writer,
                  base::OnceClosure callback,
                  const std::string& mime_type,
                  std::u16string data);
  void OnImageRead(scoped_refptr<RefCountedScopedClipboardWriter> writer,
                   base::OnceClosure callback,
                   const std::string& mime_type,
                   const std::vector<uint8_t>& data);
  void OnImageDecoded(base::OnceClosure callback,
                      scoped_refptr<RefCountedScopedClipboardWriter> writer,
                      const SkBitmap& bitmap);
  void OnFilenamesRead(ui::EndpointType source,
                       scoped_refptr<RefCountedScopedClipboardWriter> writer,
                       base::OnceClosure callback,
                       const std::string& mime_type,
                       const std::vector<uint8_t>& data);
  void OnWebCustomDataRead(
      scoped_refptr<RefCountedScopedClipboardWriter> writer,
      base::OnceClosure callback,
      const std::string& mime_type,
      const std::vector<uint8_t>& data);

  void OnAllReadsFinished(
      scoped_refptr<RefCountedScopedClipboardWriter> writer);

  // Max value of SeatObserver's priority. Both side are inclusive.
  static constexpr int kMaxObserverPriority = 1;

  // Map from priority to a list of SeatObserver pointers.
  std::array<base::ObserverList<SeatObserver>::Unchecked,
             kMaxObserverPriority + 1>
      priority_observer_list_;

  // The platform code is the key in this map as it represents the physical
  // key that was pressed. The value is a set of potentially rewritten key codes
  // that the physical key press generated.
  base::flat_map<PhysicalCode, base::flat_set<KeyState>> pressed_keys_;
  PhysicalCode physical_code_for_currently_processing_event_ =
      ui::DomCode::NONE;

  // Data source being used as a clipboard content.
  std::unique_ptr<ScopedDataSource> selection_source_;

  // TODO(oshima): Move this to DataDevice.
  base::WeakPtr<DragDropOperation> drag_drop_operation_;

  // True while Seat is updating clipboard data to selection source.
  bool changing_clipboard_data_to_selection_source_;

  bool was_shutdown_ = false;

  std::unique_ptr<UILockController> ui_lock_controller_;
  std::unique_ptr<XkbTracker> xkb_tracker_;

  std::unique_ptr<DataExchangeDelegate> data_exchange_delegate_;
  base::WeakPtrFactory<Seat> weak_ptr_factory_{this};
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SEAT_H_
