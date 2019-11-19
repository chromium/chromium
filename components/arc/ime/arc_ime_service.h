// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_IME_ARC_IME_SERVICE_H_
#define COMPONENTS_ARC_IME_ARC_IME_SERVICE_H_

#include <memory>

#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "base/macros.h"
#include "base/optional.h"
#include "components/arc/ime/arc_ime_bridge.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}  // namespace aura

namespace content {
class BrowserContext;
}  // namespace content

namespace ui {
class InputMethod;
}  // namespace ui

namespace arc {

class ArcBridgeService;

// This class implements ui::TextInputClient and makes ARC windows behave
// as a text input target in Chrome OS environment.
class ArcImeService : public KeyedService,
                      public ArcImeBridge::Delegate,
                      public aura::EnvObserver,
                      public aura::WindowObserver,
                      public aura::client::FocusChangeObserver,
                      public ash::KeyboardControllerObserver,
                      public ui::TextInputClient {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcImeService* GetForBrowserContext(content::BrowserContext* context);

  ArcImeService(content::BrowserContext* context,
                ArcBridgeService* bridge_service);
  ~ArcImeService() override;

  class ArcWindowDelegate {
   public:
    virtual ~ArcWindowDelegate() = default;
    // Checks the |window| is a transient child of an ARC window.
    // This method assumes passed |window| is already attached to window
    // hierarchy.
    virtual bool IsInArcAppWindow(const aura::Window* window) const = 0;
    virtual void RegisterFocusObserver() = 0;
    virtual void UnregisterFocusObserver() = 0;
    virtual ui::InputMethod* GetInputMethodForWindow(
        aura::Window* window) const = 0;
    virtual bool IsImeBlocked(aura::Window* window) const = 0;
  };

  // Injects the custom IPC bridge object for testing purpose only.
  void SetImeBridgeForTesting(std::unique_ptr<ArcImeBridge> test_ime_bridge);

  // Injects the custom delegate for ARC windows, for testing purpose only.
  void SetArcWindowDelegateForTesting(
      std::unique_ptr<ArcWindowDelegate> delegate);

  // Overridden from aura::EnvObserver:
  void OnWindowInitialized(aura::Window* new_window) override;

  // Overridden from aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowRemovingFromRootWindow(aura::Window* window,
                                      aura::Window* new_root) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowRemoved(aura::Window* removed_window) override;

  // Overridden from aura::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // Overridden from ArcImeBridge::Delegate:
  void OnTextInputTypeChanged(ui::TextInputType type,
                              bool is_personalized_learning_allowed,
                              int flags) override;
  void OnCursorRectChanged(const gfx::Rect& rect,
                           bool is_screen_coordinates) override;
  void OnCancelComposition() override;
  void ShowVirtualKeyboardIfEnabled() override;
  void OnCursorRectChangedWithSurroundingText(
      const gfx::Rect& rect,
      const gfx::Range& text_range,
      const base::string16& text_in_range,
      const gfx::Range& selection_range,
      bool is_screen_coordinates) override;
  void RequestHideIme() override;

  // Overridden from ash::KeyboardControllerObserver.
  void OnKeyboardAppearanceChanged(
      const ash::KeyboardStateDescriptor& state) override;

  // Overridden from ui::TextInputClient:
  void SetCompositionText(const ui::CompositionText& composition) override;
  void ConfirmCompositionText(bool keep_selection) override;
  void ClearCompositionText() override;
  void InsertText(const base::string16& text) override;
  void InsertChar(const ui::KeyEvent& event) override;
  ui::TextInputType GetTextInputType() const override;
  gfx::Rect GetCaretBounds() const override;
  bool GetTextRange(gfx::Range* range) const override;
  bool GetEditableSelectionRange(gfx::Range* range) const override;
  bool GetTextFromRange(const gfx::Range& range,
                        base::string16* text) const override;
  void EnsureCaretNotInRect(const gfx::Rect& rect) override;

  // Overridden from ui::TextInputClient (with default implementation):
  // TODO(kinaba): Support each of these methods to the extent possible in
  // Android input method API.
  ui::TextInputMode GetTextInputMode() const override;
  base::i18n::TextDirection GetTextDirection() const override;
  int GetTextInputFlags() const override;
  bool CanComposeInline() const override;
  bool GetCompositionCharacterBounds(uint32_t index,
                                     gfx::Rect* rect) const override;
  bool HasCompositionText() const override;
  FocusReason GetFocusReason() const override;
  bool GetCompositionTextRange(gfx::Range* range) const override;
  bool SetEditableSelectionRange(const gfx::Range& range) override;
  bool DeleteRange(const gfx::Range& range) override;
  void OnInputMethodChanged() override {}
  bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) override;
  void ExtendSelectionAndDelete(size_t before, size_t after) override;
  bool IsTextEditCommandEnabled(ui::TextEditCommand command) const override;
  void SetTextEditCommandForNextKeyEvent(ui::TextEditCommand command) override {
  }
  ukm::SourceId GetClientSourceForMetrics() const override;
  bool ShouldDoLearning() override;
  bool SetCompositionFromExistingText(
      const gfx::Range& range,
      const std::vector<ui::ImeTextSpan>& ui_ime_text_spans) override;

  // Normally, the default device scale factor is used to convert from DPI to
  // physical pixels. This method provides a way to override it for testing.
  static void SetOverrideDefaultDeviceScaleFactorForTesting(
      base::Optional<double> scale_factor);

 private:
  ui::InputMethod* GetInputMethod();

  // Detaches from the IME associated with the |old_window|, and attaches to the
  // IME associated with |new_window|. Called when the focus status of ARC
  // windows has changed, or when an ARC window moved to a different display.
  // Do nothing if both windows are associated with the same IME.
  void ReattachInputMethod(aura::Window* old_window, aura::Window* new_window);

  void InvalidateSurroundingTextAndSelectionRange();

  // Converts |rect| passed from the client to the host's cooridnates and
  // updates |cursor_rect_|. Returns whether or not the stored value changed.
  bool UpdateCursorRect(const gfx::Rect& rect, bool is_screen_coordinates);

  std::unique_ptr<ArcImeBridge> ime_bridge_;
  std::unique_ptr<ArcWindowDelegate> arc_window_delegate_;
  ui::TextInputType ime_type_;
  // The flag is the bit map of ui::TextInputFlags.
  int ime_flags_;
  bool is_personalized_learning_allowed_;
  gfx::Rect cursor_rect_;
  bool has_composition_text_;
  gfx::Range text_range_;
  base::string16 text_in_range_;
  gfx::Range selection_range_;

  // Return value of IsImeBlocked() last time OnWindowPropertyChanged() is
  // called. It might not be the latest blocking state.
  bool last_ime_blocked_ = false;

  aura::Window* focused_arc_window_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ArcImeService);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_IME_ARC_IME_SERVICE_H_
