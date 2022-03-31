// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TEXT_INPUT_H_
#define COMPONENTS_EXO_TEXT_INPUT_H_

#include <string>

#include "base/i18n/rtl.h"
#include "base/strings/string_piece.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ime/virtual_keyboard_controller_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"

namespace ui {
class InputMethod;
}

namespace exo {
class Surface;

// This class bridges the ChromeOS input method and a text-input context.
class TextInput : public ui::TextInputClient,
                  public ui::VirtualKeyboardControllerObserver {
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
    virtual void Commit(const std::u16string& text) = 0;

    // Set the cursor position.
    // |surrounding_text| is the current surrounding text.
    // The |selection| range is in UTF-16 offsets of the current surrounding
    // text. |selection| must be a valid range, i.e.
    // selection.IsValid() && selection.GetMax() <= surrounding_text.length().
    virtual void SetCursor(base::StringPiece16 surrounding_text,
                           const gfx::Range& selection) = 0;

    // Delete the surrounding text of the current text input.
    // |surrounding_text| is the current surrounding text.
    // The delete |range| is in UTF-16 offsets of the current surrounding text.
    // |range| must be a valid range, i.e.
    // range.IsValid() && range.GetMax() <= surrounding_text.length().
    virtual void DeleteSurroundingText(base::StringPiece16 surrounding_text,
                                       const gfx::Range& range) = 0;

    // Sends a key event.
    virtual void SendKey(const ui::KeyEvent& event) = 0;

    // Called when the text direction has changed.
    virtual void OnTextDirectionChanged(
        base::i18n::TextDirection direction) = 0;

    // Sets composition from the current surrounding text offsets.
    // Offsets in |cursor| and |range| is relative to the beginning of
    // |surrounding_text|. Offsets in |ui_ime_text_spans| is relative to the new
    // composition, i.e. relative to |range|'s start. All offsets are in UTF16,
    // and must be valid.
    virtual void SetCompositionFromExistingText(
        base::StringPiece16 surrounding_text,
        const gfx::Range& cursor,
        const gfx::Range& range,
        const std::vector<ui::ImeTextSpan>& ui_ime_text_spans) = 0;
  };

  explicit TextInput(std::unique_ptr<Delegate> delegate);
  TextInput(const TextInput&) = delete;
  TextInput& operator=(const TextInput&) = delete;
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

  // Resets the current input method state.
  void Reset();

  // Sets the surrounding text in the app.
  // |cursor_pos| is the range of |text|.
  void SetSurroundingText(const std::u16string& text,
                          const gfx::Range& cursor_pos);

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
  uint32_t ConfirmCompositionText(bool keep_selection) override;
  void ClearCompositionText() override;
  void InsertText(const std::u16string& text,
                  InsertTextCursorBehavior cursor_behavior) override;
  void InsertChar(const ui::KeyEvent& event) override;
  ui::TextInputType GetTextInputType() const override;
  ui::TextInputMode GetTextInputMode() const override;
  base::i18n::TextDirection GetTextDirection() const override;
  int GetTextInputFlags() const override;
  bool CanComposeInline() const override;
  gfx::Rect GetCaretBounds() const override;
  gfx::Rect GetSelectionBoundingBox() const override;
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
                        std::u16string* text) const override;
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
  gfx::Range GetAutocorrectRange() const override;
  gfx::Rect GetAutocorrectCharacterBounds() const override;
  bool SetAutocorrectRange(const gfx::Range& range) override;
  absl::optional<ui::GrammarFragment> GetGrammarFragment(
      const gfx::Range& range) override;
  bool ClearGrammarFragments(const gfx::Range& range) override;
  bool AddGrammarFragments(
      const std::vector<ui::GrammarFragment>& fragments) override;
  void GetActiveTextInputControlLayoutBounds(
      absl::optional<gfx::Rect>* control_bounds,
      absl::optional<gfx::Rect>* selection_bounds) override {}

  // ui::VirtualKeyboardControllerObserver:
  void OnKeyboardVisible(const gfx::Rect& keyboard_rect) override;
  void OnKeyboardHidden() override;

 private:
  void AttachInputMethod(aura::Window* window);
  void DetachInputMethod();
  void ResetCompositionTextCache();

  // Delegate to talk to actual its client.
  std::unique_ptr<Delegate> delegate_;

  // On requesting to show Virtual Keyboard, InputMethod may not be connected.
  // So, remember the request temporarily, and then on InputMethod connection
  // show the Virtual Keyboard.
  bool pending_vk_visible_ = false;

  // Window instance that this TextInput is activated against.
  aura::Window* window_ = nullptr;

  // InputMethod in Chrome OS that this TextInput is attached to.
  ui::InputMethod* input_method_ = nullptr;

  // Cache of the current caret bounding box, sent from the client.
  gfx::Rect caret_bounds_;

  // Cache of the current input field attributes sent from the client.
  ui::TextInputType input_type_ = ui::TEXT_INPUT_TYPE_NONE;
  ui::TextInputMode input_mode_ = ui::TEXT_INPUT_MODE_DEFAULT;
  int flags_ = ui::TEXT_INPUT_FLAG_NONE;
  bool should_do_learning_ = true;

  // Cache of the current surrounding text, sent from the client.
  std::u16string surrounding_text_;

  // Cache of the current cursor position in the surrounding text, sent from
  // the client. Maybe "invalid" value, if not available.
  gfx::Range cursor_pos_ = gfx::Range::InvalidRange();

  // Cache of the current composition range (set in absolute indices).
  gfx::Range composition_range_ = gfx::Range::InvalidRange();

  // Cache of the current composition, updated from Chrome OS IME.
  ui::CompositionText composition_;

  // Cache of the current text input direction, update from the Chrome OS IME.
  base::i18n::TextDirection direction_ = base::i18n::UNKNOWN_DIRECTION;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_TEXT_INPUT_H_
