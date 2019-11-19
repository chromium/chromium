// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SEAT_H_
#define COMPONENTS_EXO_SEAT_H_

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/exo/data_source_observer.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/events/event_handler.h"
#include "ui/events/keycodes/dom/dom_codes.h"
#include "ui/events/platform/platform_event_observer.h"

namespace ui {
enum class DomCode;
class KeyEvent;
}  // namespace ui

namespace exo {
class DragDropOperation;
class ScopedDataSource;
class SeatObserver;
class Surface;

// The maximum number of different data types that we will write to the
// clipboard (plain text, RTF, HTML, image)
constexpr int kMaxClipboardDataTypes = 4;

// Seat object represent a group of input devices such as keyboard, pointer and
// touch devices and keeps track of input focus.
class Seat : public aura::client::FocusChangeObserver,
             public ui::PlatformEventObserver,
             public ui::EventHandler,
             public ui::ClipboardObserver,
             public DataSourceObserver {
 public:
  Seat();
  ~Seat() override;

  void AddObserver(SeatObserver* observer);
  void RemoveObserver(SeatObserver* observer);

  // Returns currently focused surface. This is vertual so that we can override
  // the behavior for testing.
  virtual Surface* GetFocusedSurface();

  // Returns currently pressed keys.
  const base::flat_map<ui::DomCode, ui::DomCode>& pressed_keys() const {
    return pressed_keys_;
  }

  // Returns current set of modifier flags.
  int modifier_flags() const { return modifier_flags_; }

  // Returns physical code for the currently processing event.
  ui::DomCode physical_code_for_currently_processing_event() const {
    return physical_code_for_currently_processing_event_;
  }

  // Sets clipboard data from |source|.
  void SetSelection(DataSource* source);

  void StartDrag(DataSource* source,
                 Surface* origin,
                 Surface* icon,
                 ui::DragDropTypes::DragEventSource event_source);

  // Abort any drag operations that haven't been started yet.
  void AbortPendingDragOperation();

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

  void set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode physical_code_for_currently_processing_event) {
    physical_code_for_currently_processing_event_ =
        physical_code_for_currently_processing_event;
  }

  base::WeakPtr<DragDropOperation> get_drag_drop_operation_for_testing() {
    return drag_drop_operation_;
  }

 private:
  class RefCountedScopedClipboardWriter
      : public ui::ScopedClipboardWriter,
        public base::RefCounted<RefCountedScopedClipboardWriter> {
   public:
    RefCountedScopedClipboardWriter(ui::ClipboardBuffer buffer)
        : ScopedClipboardWriter(buffer) {}

   private:
    friend class base::RefCounted<RefCountedScopedClipboardWriter>;
    virtual ~RefCountedScopedClipboardWriter() = default;
  };

  // Called when data is read from FD passed from a client.
  // |data| is read data. |source| is source of the data, or nullptr if
  // DataSource has already been destroyed.
  void OnTextRead(scoped_refptr<RefCountedScopedClipboardWriter> writer,
                  base::OnceClosure callback,
                  const std::string& mime_type,
                  base::string16 data);
  void OnRTFRead(scoped_refptr<RefCountedScopedClipboardWriter> writer,
                 base::OnceClosure callback,
                 const std::string& mime_type,
                 const std::vector<uint8_t>& data);
  void OnHTMLRead(scoped_refptr<RefCountedScopedClipboardWriter> writer,
                  base::OnceClosure callback,
                  const std::string& mime_type,
                  base::string16 data);
  void OnImageRead(scoped_refptr<RefCountedScopedClipboardWriter> writer,
                   base::OnceClosure callback,
                   const std::string& mime_type,
                   const std::vector<uint8_t>& data);
#if defined(OS_CHROMEOS)
  void OnImageDecoded(base::OnceClosure callback,
                      scoped_refptr<RefCountedScopedClipboardWriter> writer,
                      const SkBitmap& bitmap);
#endif  // defined(OS_CHROMEOS)

  void OnAllReadsFinished(
      scoped_refptr<RefCountedScopedClipboardWriter> writer);

  base::ObserverList<SeatObserver>::Unchecked observers_;
  // The platform code is the key in this map as it represents the physical
  // key that was pressed. The value is a potentially rewritten code that the
  // physical key press generated.
  base::flat_map<ui::DomCode, ui::DomCode> pressed_keys_;
  ui::DomCode physical_code_for_currently_processing_event_ = ui::DomCode::NONE;
  int modifier_flags_ = 0;

  // Data source being used as a clipboard content.
  std::unique_ptr<ScopedDataSource> selection_source_;

  base::WeakPtr<DragDropOperation> drag_drop_operation_;

  // True while Seat is updating clipboard data to selection source.
  bool changing_clipboard_data_to_selection_source_;

  base::WeakPtrFactory<Seat> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Seat);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SEAT_H_
