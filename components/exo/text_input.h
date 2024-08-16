// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TEXT_INPUT_H_
#define COMPONENTS_EXO_TEXT_INPUT_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/exo/seat_observer.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/autocorrect_info.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/surrounding_text_tracker.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ime/virtual_keyboard_controller.h"
#include "ui/base/ime/virtual_keyboard_controller_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"

namespace ui {
class InputMethod;
}  // namespace ui

namespace exo {
class Surface;
class Seat;

// This class bridges the ChromeOS input method and a text-input context.
// It can be inactive, active, or in a pending state where Activate() was
// called but the associated window is not focused.
class TextInput : public ui::TextInputClient,
                  public ui::VirtualKeyboardControllerObserver,
                  public ash::input_method::InputMethodManager::Observer,
                  public SeatObserver {
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

    // Called when the virtual keyboard's occluded bounds has changed.
    // The bounds are in screen DIP.
    virtual void OnVirtualKeyboardOccludedBoundsChanged(
        const gfx::Rect& screen_bounds) = 0;

    // Returns true if the server can expect a finalize_virtual_keyboard_changes
    // request from the client.
    virtual bool SupportsFinalizeVirtualKeyboardChanges() = 0;

    // Set the 'composition text' of the current text input.
    virtual void SetCompositionText(const ui::CompositionText& composition) = 0;

    // Commit |text| to the current text input session.
    virtual void Commit(std::u16string_view text) = 0;

    // Set the cursor position.
    // |surrounding_text| is the current surrounding text.
    // The |selection| range is in UTF-16 offsets of the current surrounding
    // text. |selection| must be a valid range, i.e.
    // selection.IsValid() && selection.GetMax() <= surrounding_text.length().
    virtual void SetCursor(std::u16string_view surrounding_text,
                           const gfx::Range& selection) = 0;

    // Delete the surrounding text of the current text input.
    // |surrounding_text| is the current surrounding text.
    // The delete |range| is in UTF-16 offsets of the current surrounding text.
    // |range| must be a valid range, i.e.
    // range.IsValid() && range.GetMax() <= surrounding_text.length().
    virtual void DeleteSurroundingText(std::u16string_view surrounding_text,
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
        std::u16string_view surrounding_text,
        const gfx::Range& cursor,
        const gfx::Range& range,
        const std::vector<ui::ImeTextSpan>& ui_ime_text_spans) = 0;

    // Clears all the grammar fragments in |range|.
    // |surrounding_text| is the current surrounding text, used for utf16 to
    // utf8 conversion.
    virtual void ClearGrammarFragments(std::u16string_view surrounding_text,
                                       const gfx::Range& range) = 0;

    // Adds a new grammar marker according to |fragments|. Clients should show
    // some visual indications such as underlining.
    // |surrounding_text| is the current surrounding text, used for utf16 to
    // utf8 conversion.
    virtual void AddGrammarFragment(std::u16string_view surrounding_text,
                                    const ui::GrammarFragment& fragment) = 0;

    // Sets the autocorrect range from the current surrounding text offsets.
    // Offsets in |range| is relative to the beginning of
    // |surrounding_text|. All offsets are in UTF16, and must be valid.
    virtual void SetAutocorrectRange(std::u16string_view surrounding_text,
                                     const gfx::Range& range) = 0;

    // Commits the current composition text.
    // If `keep_selection` is true, keep the selection range unchanged.
    // Otherwise, set the selection range to be after the committed text.
    // Returns whether the operation is supported by the client.
    virtual bool ConfirmComposition(bool keep_selection) = 0;

    // Does the current delegate support the new ConfirmComposition wayland
    // method name confirm_preedit?
    virtual bool SupportsConfirmPreedit() = 0;

    // Checks if InsertImage() is supported via wayland.
    virtual bool HasImageInsertSupport() = 0;

    // Inserts image.
    virtual void InsertImage(const GURL& src) = 0;
  };

  explicit TextInput(std::unique_ptr<Delegate> delegate);
  TextInput(const TextInput&) = delete;
  TextInput& operator=(const TextInput&) = delete;
  ~TextInput() override;

  // Request to activate the text input context on the surface. Activation will
  // occur immediately if the associated window is already focused, or
  // otherwise when the window gains focus.
  void Activate(Seat* seat,
                Surface* surface,
                ui::TextInputClient::FocusReason reason);

  // Deactivates the text input context.
  void Deactivate();

  // Shows the virtual keyboard if needed.
  void ShowVirtualKeyboardIfEnabled();

  // Hides the virtual keyboard.
  void HideVirtualKeyboard();

  // Re-synchronize the current status when the surrounding text has changed
  // during the text input session.
  void Resync();

  // Resets the current input method composition state.
  void Reset();

  // Sets the surrounding text in the app.
  // Ranges of |cursor_pos|, |grammar_fragment| and |autocorrect_info| are
  // relative to |text|.
  // |grammar_fragment| is the grammar fragment at the cursor position,
  // if given.
  // |autocorrect_info->bounds| is the bounding rect around the autocorrected
  // text and is relative to the window origin.
  void SetSurroundingText(
      std::u16string_view text,
      uint32_t offset,
      const gfx::Range& cursor_pos,
      const std::optional<ui::GrammarFragment>& grammar_fragment,
      const std::optional<ui::AutocorrectInfo>& autocorrect_info);

  // Sets the text input type, mode, flags, |should_do_learning|,
  // |can_compose_inline| and |surrounding_text_supported|.
  void SetTypeModeFlags(ui::TextInputType type,
                        ui::TextInputMode mode,
                        int flags,
                        bool should_do_learning,
                        bool can_compose_inline,
                        bool surrounding_text_supported);

  // Sets the bounds of the text caret, relative to the window origin.
  void SetCaretBounds(const gfx::Rect& bounds);

  // Finalizes pending virtual keyboard requested changes.
  void FinalizeVirtualKeyboardChanges();

  Delegate* delegate() { return delegate_.get(); }

  // ui::TextInputClient:
  base::WeakPtr<ui::TextInputClient> AsWeakPtr() override;
  void SetCompositionText(const ui::CompositionText& composition) override;
  size_t ConfirmCompositionText(bool keep_selection) override;
  void ClearCompositionText() override;
  void InsertText(const std::u16string& text,
                  InsertTextCursorBehavior cursor_behavior) override;
  void InsertChar(const ui::KeyEvent& event) override;
  bool CanInsertImage() override;
  void InsertImage(const GURL& src) override;
  ui::TextInputType GetTextInputType() const override;
  ui::TextInputMode GetTextInputMode() const override;
  base::i18n::TextDirection GetTextDirection() const override;
  int GetTextInputFlags() const override;
  bool CanComposeInline() const override;
  gfx::Rect GetCaretBounds() const override;
  gfx::Rect GetSelectionBoundingBox() const override;
  bool GetCompositionCharacterBounds(size_t index,
                                     gfx::Rect* rect) const override;
  bool HasCompositionText() const override;
  ui::TextInputClient::FocusReason GetFocusReason() const override;
  bool GetTextRange(gfx::Range* range) const override;
  bool GetCompositionTextRange(gfx::Range* range) const override;
  bool GetEditableSelectionRange(gfx::Range* range) const override;
  bool SetEditableSelectionRange(const gfx::Range& range) override;
  bool GetTextFromRange(const gfx::Range& range,
                        std::u16string* text) const override;
  void OnInputMethodChanged() override;
  bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) override;
  void ExtendSelectionAndDelete(size_t before, size_t after) override;
  void ExtendSelectionAndReplace(size_t before,
                                 size_t after,
                                 std::u16string_view replacement_text) override;
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
  std::optional<ui::GrammarFragment> GetGrammarFragmentAtCursor()
      const override;
  bool ClearGrammarFragments(const gfx::Range& range) override;
  bool AddGrammarFragments(
      const std::vector<ui::GrammarFragment>& fragments) override;
  bool SupportsAlwaysConfirmComposition() override;
  void GetActiveTextInputControlLayoutBounds(
      std::optional<gfx::Rect>* control_bounds,
      std::optional<gfx::Rect>* selection_bounds) override {}

  // ui::VirtualKeyboardControllerObserver:
  void OnKeyboardVisible(const gfx::Rect& keyboard_rect) override;
  void OnKeyboardHidden() override;

  // ash::input_method::InputMethodManager::Observer:
  void InputMethodChanged(ash::input_method::InputMethodManager* manager,
                          Profile* profile,
                          bool show_message) override;

  // SeatObserver:
  void OnSurfaceFocused(Surface* gained_focus,
                        Surface* lost_focus,
                        bool has_focused_surface) override;

 private:
  void AttachInputMethod();
  void DetachInputMethod();
  void ResetCompositionTextCache();

  bool ShouldStageVKState();
  void SendStagedVKVisibility();
  void SendStagedVKOccludedBounds();

  // Delegate to talk to actual its client.
  std::unique_ptr<Delegate> delegate_;

  // On requesting to show Virtual Keyboard, InputMethod may not be connected.
  // So, remember the request temporarily, and then on InputMethod connection
  // show the Virtual Keyboard.
  bool pending_vk_visible_ = false;

  // |surface_| and |seat_| are non-null if and only if the TextInput is in a
  // pending or active state, in which case the TextInput will be observing the
  // Seat.
  raw_ptr<Surface, DanglingUntriaged> surface_ = nullptr;
  raw_ptr<Seat, DanglingUntriaged> seat_ = nullptr;

  // If the TextInput is active (associated window has focus) and the
  // InputMethod is available, this is set and the TextInput will be its
  // focused client. Otherwise, it is null and the TextInput is not attached
  // to any InputMethod, so the TextInputClient overrides will not be called.
  raw_ptr<ui::InputMethod, DanglingUntriaged> input_method_ = nullptr;

  base::ScopedObservation<ash::input_method::InputMethodManager,
                          ash::input_method::InputMethodManager::Observer>
      input_method_manager_observation_{this};

  base::ScopedObservation<ui::VirtualKeyboardController,
                          ui::VirtualKeyboardControllerObserver>
      virtual_keyboard_observation_{this};

  // Cache of the current caret bounding box, sent from the client.
  gfx::Rect caret_bounds_;

  // Cache of the current input field attributes sent from the client.
  ui::TextInputType input_type_ = ui::TEXT_INPUT_TYPE_NONE;
  ui::TextInputMode input_mode_ = ui::TEXT_INPUT_MODE_DEFAULT;
  int flags_ = ui::TEXT_INPUT_FLAG_NONE;
  bool should_do_learning_ = true;
  bool can_compose_inline_ = true;
  ui::TextInputClient::FocusReason focus_reason_ =
      ui::TextInputClient::FOCUS_REASON_NONE;

  // Whether the client supports surrounding text.
  bool surrounding_text_supported_ = true;
  // If surrounding text is not supported and the active IME needs it, we force
  // using TEXT_INPUT_TYPE_NULL.
  bool use_null_input_type_ = false;

  // Tracks the surrounding text.
  ui::SurroundingTextTracker surrounding_text_tracker_;

  // Cache of the current text input direction, update from the Chrome OS IME.
  base::i18n::TextDirection direction_ = base::i18n::UNKNOWN_DIRECTION;

  // Cache of the grammar fragment at cursor position, send from Lacros side.
  std::optional<ui::GrammarFragment> grammar_fragment_at_cursor_;

  // Latest autocorrect information that was sent from the Wayland client.
  // along with the last surrounding text change.
  ui::AutocorrectInfo autocorrect_info_;

  // True when client has made virtual keyboard related requests but haven't
  // sent the virtual keyboard finalize request.
  bool pending_vk_finalize_ = false;
  // Holds the vk visibility to send to the client.
  std::optional<bool> staged_vk_visible_;
  // Holds the vk occluded bounds to send to the client.
  std::optional<gfx::Rect> staged_vk_occluded_bounds_;
  base::WeakPtrFactory<TextInput> weak_ptr_factory_{this};
};

}  // namespace exo

#endif  // COMPONENTS_EXO_TEXT_INPUT_H_
