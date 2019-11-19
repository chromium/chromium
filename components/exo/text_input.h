// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TEXT_INPUT_H_
#define COMPONENTS_EXO_TEXT_INPUT_H_

#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "base/optional.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
class InputMethod;
}

namespace keyboard {
class KeyboardUIController;
}

namespace exo {
class Surface;

size_t OffsetFromUTF8Offset(const base::StringPiece& text, uint32_t offset);
size_t OffsetFromUTF16Offset(const base::StringPiece16& text, uint32_t offset);

// This class bridges the ChromeOS input method and a text-input context.
class TextInput : public ui::TextInputClient,
                  public ash::KeyboardControllerObserver {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when the text input session is activated.
    virtual void Activated() = 0;

    // Called when the text input session is deactivated. TextInput does not
    // refer to the delegate anymore.
    virtual void Deactivated() = 0;

    // Called when the virtual keyboard visibility state has changed.
    virtual void OnVirtualKeyboardVisibilityChanged(bool is_visible) = 0;

    // Set the 'composition text' of the current text input.
    virtual void SetCompositionText(const ui::CompositionText& composition) = 0;

    // Commit |text| to the current text input session.
    virtual void Commit(const base::string16& text) = 0;

    // Set the cursor position. The range should be in bytes offset.
    virtual void SetCursor(const gfx::Range& selection) = 0;

    // Delete the surrounding text of the current text input. The range should
    // be in the bytes offset.
    virtual void DeleteSurroundingText(const gfx::Range& range) = 0;

    // Sends a key event.
    virtual void SendKey(const ui::KeyEvent& event) = 0;

    // Called when the text direction has changed.
    virtual void OnTextDirectionChanged(
        base::i18n::TextDirection direction) = 0;
  };

  TextInput(std::unique_ptr<Delegate> delegate);
  ~TextInput() override;

  // Activates the text input context on the surface. Note that surface can be
  // an app surface (hosted by a shell surface) or can be an independent one
  // created by the text-input client.
  void Activate(Surface* surface);

  // Deactivates the text input context.
  void Deactivate();

  // Shows the virtual keyboard if needed.
  void ShowVirtualKeyboardIfEnabled();

  // Hides the virtual keyboard.
  void HideVirtualKeyboard();

  // Re-synchronize the current status when the surrounding text has changed
  // during the text input session.
  void Resync();

  // Sets the surrounding text in the app.
  void SetSurroundingText(const base::string16& text,
                          uint32_t cursor_pos,
                          uint32_t anchor);

  // Sets the text input type, mode, flags, and |should_do_learning|.
  void SetTypeModeFlags(ui::TextInputType type,
                        ui::TextInputMode mode,
                        int flags,
                        bool should_do_learning);

  // Sets the bounds of the text caret, relative to the window origin.
  void SetCaretBounds(const gfx::Rect& bounds);

  Delegate* delegate() { return delegate_.get(); }

  // ui::TextInputClient:
  void SetCompositionText(const ui::CompositionText& composition) override;
  void ConfirmCompositionText(bool keep_selection) override;
  void ClearCompositionText() override;
  void InsertText(const base::string16& text) override;
  void InsertChar(const ui::KeyEvent& event) override;
  ui::TextInputType GetTextInputType() const override;
  ui::TextInputMode GetTextInputMode() const override;
  base::i18n::TextDirection GetTextDirection() const override;
  int GetTextInputFlags() const override;
  bool CanComposeInline() const override;
  gfx::Rect GetCaretBounds() const override;
  bool GetCompositionCharacterBounds(uint32_t index,
                                     gfx::Rect* rect) const override;
  bool HasCompositionText() const override;
  ui::TextInputClient::FocusReason GetFocusReason() const override;
  bool GetTextRange(gfx::Range* range) const override;
  bool GetCompositionTextRange(gfx::Range* range) const override;
  bool GetEditableSelectionRange(gfx::Range* range) const override;
  bool SetEditableSelectionRange(const gfx::Range& range) override;
  bool DeleteRange(const gfx::Range& range) override;
  bool GetTextFromRange(const gfx::Range& range,
                        base::string16* text) const override;
  void OnInputMethodChanged() override;
  bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) override;
  void ExtendSelectionAndDelete(size_t before, size_t after) override;
  void EnsureCaretNotInRect(const gfx::Rect& rect) override;
  bool IsTextEditCommandEnabled(ui::TextEditCommand command) const override;
  void SetTextEditCommandForNextKeyEvent(ui::TextEditCommand command) override;
  ukm::SourceId GetClientSourceForMetrics() const override;
  bool ShouldDoLearning() override;
  bool SetCompositionFromExistingText(
      const gfx::Range& range,
      const std::vector<ui::ImeTextSpan>& ui_ime_text_spans) override;

  // ash::KeyboardControllerObserver:
  void OnKeyboardVisibilityChanged(bool is_visible) override;

 private:
  void AttachInputMethod();
  void DetachInputMethod();

  std::unique_ptr<Delegate> delegate_;
  keyboard::KeyboardUIController* keyboard_ui_controller_ = nullptr;

  bool pending_vk_visible_ = false;
  aura::Window* window_ = nullptr;
  ui::InputMethod* input_method_ = nullptr;
  gfx::Rect caret_bounds_;
  ui::TextInputType input_type_ = ui::TEXT_INPUT_TYPE_NONE;
  ui::TextInputMode input_mode_ = ui::TEXT_INPUT_MODE_DEFAULT;
  int flags_ = ui::TEXT_INPUT_FLAG_NONE;
  bool should_do_learning_ = true;
  ui::CompositionText composition_;
  base::string16 surrounding_text_;
  base::Optional<gfx::Range> cursor_pos_;
  base::i18n::TextDirection direction_ = base::i18n::UNKNOWN_DIRECTION;

  DISALLOW_COPY_AND_ASSIGN(TextInput);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_KEYBOARD_H_
