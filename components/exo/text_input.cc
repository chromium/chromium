// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/text_input.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "components/exo/seat.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/utf_offset.h"
#include "ui/base/ime/virtual_keyboard_controller.h"
#include "ui/events/event.h"

namespace exo {

namespace {

constexpr int kTextInputSeatObserverPriority = 1;
static_assert(Seat::IsValidObserverPriority(kTextInputSeatObserverPriority),
              "kTextInputSeatObserverPriority is not in the valid range.");

ui::InputMethod* GetInputMethod(aura::Window* window) {
  if (!window || !window->GetHost())
    return nullptr;
  return window->GetHost()->GetInputMethod();
}

bool ShouldUseNullInputType(bool surrounding_text_supported) {
  // TODO(b/273674108): We should be able to tell the IME that the client does
  // not support surrounding text. Instead, we currently disable all IME
  // features by setting input type to null in cases where the IME will not
  // function correctly without surrounding text.
  // Some basic IMEs (incl. EN, DE, FR) are known to be buggy when auto-correct
  // is on and surrounding text is not provided.
  // Complex IMEs (e.g. JA, KO) are not known to be buggy when surrounding text
  // is not provided.

  if (surrounding_text_supported) {
    return false;
  }

  auto* manager = ash::input_method::InputMethodManager::Get();
  scoped_refptr<ash::input_method::InputMethodManager::State> state =
      manager->GetActiveIMEState();
  if (!state) {
    return false;
  }

  return state->GetCurrentInputMethod().id().find("xkb:") != std::string::npos;
}

gfx::Range RemoveOffset(gfx::Range range, size_t offset) {
  return {range.start() - offset, range.end() - offset};
}

}  // namespace

TextInput::TextInput(std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {
  input_method_manager_observation_.Observe(
      ash::input_method::InputMethodManager::Get());
}

TextInput::~TextInput() {
  Deactivate();
}

void TextInput::Activate(Seat* seat,
                         Surface* surface,
                         ui::TextInputClient::FocusReason reason) {
  DCHECK(surface);
  DCHECK(seat);

  focus_reason_ = reason;
  if (surface_ == surface)
    return;
  DetachInputMethod();
  surface_ = surface;
  seat_ = seat;
  seat_->AddObserver(this, kTextInputSeatObserverPriority);
  if (seat_->GetFocusedSurface() == surface_)
    AttachInputMethod();
}

void TextInput::Deactivate() {
  focus_reason_ = ui::TextInputClient::FOCUS_REASON_NONE;
  if (!surface_)
    return;
  DetachInputMethod();
  seat_->RemoveObserver(this);
  surface_ = nullptr;
  seat_ = nullptr;
}

void TextInput::ShowVirtualKeyboardIfEnabled() {
  pending_vk_finalize_ = true;

  // Some clients may ask showing virtual keyboard before sending activation.
  if (!input_method_) {
    pending_vk_visible_ = true;
    return;
  }
  input_method_->SetVirtualKeyboardVisibilityIfEnabled(true);
}

void TextInput::HideVirtualKeyboard() {
  pending_vk_finalize_ = true;

  if (input_method_)
    input_method_->SetVirtualKeyboardVisibilityIfEnabled(false);
  pending_vk_visible_ = false;
}

void TextInput::Resync() {
  if (input_method_)
    input_method_->OnCaretBoundsChanged(this);
}

void TextInput::Reset() {
  surrounding_text_tracker_.CancelComposition();
  if (input_method_)
    input_method_->CancelComposition(this);
}

void TextInput::SetSurroundingText(
    std::u16string_view text,
    uint32_t offset,
    const gfx::Range& cursor_pos,
    const std::optional<ui::GrammarFragment>& grammar_fragment,
    const std::optional<ui::AutocorrectInfo>& autocorrect_info) {
  surrounding_text_tracker_.Update(text, offset, cursor_pos);

  grammar_fragment_at_cursor_ = grammar_fragment;
  if (autocorrect_info.has_value()) {
    autocorrect_info_ = autocorrect_info.value();
  }

  // TODO(b/206068262): Consider introducing an API to notify surrounding text
  // update explicitly.
  if (input_method_)
    input_method_->OnCaretBoundsChanged(this);
}

void TextInput::SetTypeModeFlags(ui::TextInputType type,
                                 ui::TextInputMode mode,
                                 int flags,
                                 bool should_do_learning,
                                 bool can_compose_inline,
                                 bool surrounding_text_supported) {
  if (!input_method_) {
    return;
  }

  bool changed = (input_type_ != type) || (input_mode_ != mode) ||
                 (flags_ != flags) ||
                 (should_do_learning_ != should_do_learning) ||
                 (can_compose_inline_ != can_compose_inline) ||
                 (surrounding_text_supported_ != surrounding_text_supported);
  input_type_ = type;
  input_mode_ = mode;
  flags_ = flags;
  should_do_learning_ = should_do_learning;
  can_compose_inline_ = can_compose_inline;
  surrounding_text_supported_ = surrounding_text_supported;
  use_null_input_type_ = ShouldUseNullInputType(surrounding_text_supported_);
  if (changed)
    input_method_->OnTextInputTypeChanged(this);
}

void TextInput::SetCaretBounds(const gfx::Rect& bounds) {
  if (caret_bounds_ == bounds)
    return;
  caret_bounds_ = bounds;
  if (!input_method_)
    return;
  input_method_->OnCaretBoundsChanged(this);
}

void TextInput::FinalizeVirtualKeyboardChanges() {
  if (staged_vk_visible_) {
    // Order the events so vk bounds is sent while vk is visible.
    if (*staged_vk_visible_) {
      SendStagedVKVisibility();
      SendStagedVKOccludedBounds();
    } else {
      SendStagedVKOccludedBounds();
      SendStagedVKVisibility();
    }
  }

  if (staged_vk_occluded_bounds_) {
    SendStagedVKOccludedBounds();
  }

  pending_vk_finalize_ = false;
}

base::WeakPtr<ui::TextInputClient> TextInput::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void TextInput::SetCompositionText(const ui::CompositionText& composition) {
  delegate_->SetCompositionText(composition);
  surrounding_text_tracker_.OnSetCompositionText(composition);
}

size_t TextInput::ConfirmCompositionText(bool keep_selection) {
  const auto& predicted_state = surrounding_text_tracker_.predicted_state();
  const auto& [surrounding_text, utf16_offset, cursor_pos, composition] =
      predicted_state;

  if (!delegate_->ConfirmComposition(keep_selection)) {
    // Fallback to SetCursor and Commit if ConfirmComposition is not supported.
    // TODO(b/265853952): Remove once all versions of Lacros supports
    // ConfirmComposition.
    if (keep_selection && cursor_pos.IsValid() &&
        cursor_pos.IsBoundedBy(predicted_state.GetSurroundingTextRange())) {
      delegate_->SetCursor(surrounding_text,
                           RemoveOffset(cursor_pos, utf16_offset));
    }

    delegate_->Commit(
        predicted_state.GetCompositionText().value_or(std::u16string_view()));
  }

  // Preserve the result value before updating the tracker's state.
  const size_t composition_text_length = composition.length();
  surrounding_text_tracker_.OnConfirmCompositionText(keep_selection);
  return composition_text_length;
}

void TextInput::ClearCompositionText() {
  const auto composition =
      surrounding_text_tracker_.predicted_state().composition;
  if (composition.is_empty())
    return;
  delegate_->SetCompositionText(ui::CompositionText{});
  surrounding_text_tracker_.OnClearCompositionText();
}

void TextInput::InsertText(const std::u16string& text,
                           InsertTextCursorBehavior cursor_behavior) {
  // TODO(crbug.com/1155331): Handle |cursor_behavior| correctly.
  delegate_->Commit(text);
  surrounding_text_tracker_.OnInsertText(
      text, InsertTextCursorBehavior::kMoveCursorAfterText);
}

void TextInput::InsertChar(const ui::KeyEvent& event) {
  if (ConsumedByIme(event)) {
    // TODO(b/240618514): Short term workaround to accept temporary fix in IME
    // for urgent production breakage.
    // We should come up with the proper solution of what to be done.
    if (event.code() == ui::DomCode::NONE) {
      // On some specific cases, IME use InsertChar, even if there's no clear
      // key mapping from key_code. Then, use InsertText().
      InsertText(std::u16string(1u, event.GetCharacter()),
                 InsertTextCursorBehavior::kMoveCursorAfterText);
    } else {
      delegate_->SendKey(event);
    }
  }
}

bool TextInput::CanInsertImage() {
  return delegate_->HasImageInsertSupport() &&
         input_type_ == ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE;
}

void TextInput::InsertImage(const GURL& src) {
  if (CanInsertImage()) {
    delegate_->InsertImage(src);
  }
}

ui::TextInputType TextInput::GetTextInputType() const {
  return use_null_input_type_ ? ui::TEXT_INPUT_TYPE_NULL : input_type_;
}

ui::TextInputMode TextInput::GetTextInputMode() const {
  return input_mode_;
}

base::i18n::TextDirection TextInput::GetTextDirection() const {
  return direction_;
}

int TextInput::GetTextInputFlags() const {
  return flags_;
}

bool TextInput::CanComposeInline() const {
  return can_compose_inline_;
}

gfx::Rect TextInput::GetCaretBounds() const {
  return caret_bounds_ +
         surface_->window()->GetBoundsInScreen().OffsetFromOrigin();
}

gfx::Rect TextInput::GetSelectionBoundingBox() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

bool TextInput::GetCompositionCharacterBounds(size_t index,
                                              gfx::Rect* rect) const {
  return false;
}

bool TextInput::HasCompositionText() const {
  return !surrounding_text_tracker_.predicted_state().composition.is_empty();
}

ui::TextInputClient::FocusReason TextInput::GetFocusReason() const {
  return focus_reason_;
}

bool TextInput::GetTextRange(gfx::Range* range) const {
  DCHECK(range);
  const auto& predicted_state = surrounding_text_tracker_.predicted_state();
  DCHECK(predicted_state.selection.IsValid());

  *range = predicted_state.GetSurroundingTextRange();
  return true;
}

bool TextInput::GetCompositionTextRange(gfx::Range* range) const {
  DCHECK(range);
  const auto& composition =
      surrounding_text_tracker_.predicted_state().composition;
  if (composition.is_empty())
    return false;

  *range = composition;
  return true;
}

bool TextInput::GetEditableSelectionRange(gfx::Range* range) const {
  DCHECK(range);
  const auto& selection = surrounding_text_tracker_.predicted_state().selection;
  DCHECK(selection.IsValid());

  *range = selection;
  return true;
}

bool TextInput::SetEditableSelectionRange(const gfx::Range& range) {
  const auto& predicted_state = surrounding_text_tracker_.predicted_state();
  std::optional<std::u16string_view> composition_text =
      predicted_state.GetCompositionText();
  if (!range.IsBoundedBy(predicted_state.GetSurroundingTextRange()) ||
      !composition_text.has_value()) {
    return false;
  }

  // Send a SetCursor followed by a Commit of the current composition text, or
  // empty string if there is no composition text. This is necessary since
  // SetCursor only takes effect on the following Commit.
  delegate_->SetCursor(predicted_state.surrounding_text,
                       RemoveOffset(range, predicted_state.utf16_offset));
  delegate_->Commit(*composition_text);
  surrounding_text_tracker_.OnSetEditableSelectionRange(range);
  return true;
}

bool TextInput::GetTextFromRange(const gfx::Range& range,
                                 std::u16string* text) const {
  DCHECK(text);
  const auto& predicted_state = surrounding_text_tracker_.predicted_state();
  if (!range.IsBoundedBy(predicted_state.GetSurroundingTextRange())) {
    return false;
  }

  text->assign(predicted_state.surrounding_text,
               range.GetMin() - predicted_state.utf16_offset, range.length());
  return true;
}

void TextInput::OnInputMethodChanged() {
  // This observer method does not signify anything meaningful. When the user
  // switches input method, |InputMethodChanged()| is triggered instead of
  // this, and the ui::InputMethod we are attached to is a singleton which does
  // not change.
}

bool TextInput::ChangeTextDirectionAndLayoutAlignment(
    base::i18n::TextDirection direction) {
  if (direction == direction_)
    return true;
  direction_ = direction;
  delegate_->OnTextDirectionChanged(direction_);
  return true;
}

void TextInput::ExtendSelectionAndDelete(size_t before, size_t after) {
  const auto& [surrounding_text, utf16_offset, selection, unused_composition] =
      surrounding_text_tracker_.predicted_state();

  DCHECK(selection.IsValid());

  size_t utf16_start =
      selection.GetMin() - std::min(before, selection.GetMin());
  size_t utf16_end = std::min(selection.GetMax() + after,
                              surrounding_text.length() + utf16_offset);

  delegate_->DeleteSurroundingText(
      surrounding_text,
      gfx::Range(utf16_start - utf16_offset, utf16_end - utf16_offset));
  surrounding_text_tracker_.OnExtendSelectionAndDelete(before, after);
}

void TextInput::ExtendSelectionAndReplace(
    size_t before,
    size_t after,
    const std::u16string_view replacement_text) {
  // TODO(crbug.com/40267455): Implement this using an extended Wayland API.
  NOTIMPLEMENTED_LOG_ONCE();
}

void TextInput::EnsureCaretNotInRect(const gfx::Rect& rect) {
  if (ShouldStageVKState()) {
    staged_vk_occluded_bounds_ = rect;
    return;
  }
  delegate_->OnVirtualKeyboardOccludedBoundsChanged(rect);
}

bool TextInput::IsTextEditCommandEnabled(ui::TextEditCommand command) const {
  return false;
}

void TextInput::SetTextEditCommandForNextKeyEvent(ui::TextEditCommand command) {
}

ukm::SourceId TextInput::GetClientSourceForMetrics() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return ukm::kInvalidSourceId;
}

bool TextInput::ShouldDoLearning() {
  return should_do_learning_;
}

bool TextInput::SetCompositionFromExistingText(
    const gfx::Range& range,
    const std::vector<ui::ImeTextSpan>& ui_ime_text_spans) {
  const auto& predicted_state = surrounding_text_tracker_.predicted_state();
  const gfx::Range surrounding_text_range =
      predicted_state.GetSurroundingTextRange();
  DCHECK(predicted_state.selection.IsValid());
  if (!range.IsBoundedBy(surrounding_text_range) ||
      !predicted_state.selection.IsBoundedBy(surrounding_text_range)) {
    return false;
  }

  const auto composition_length = range.length();
  for (const auto& span : ui_ime_text_spans) {
    if (composition_length < std::max(span.start_offset, span.end_offset)) {
      return false;
    }
  }

  const size_t utf16_offset = predicted_state.utf16_offset;
  delegate_->SetCompositionFromExistingText(
      predicted_state.surrounding_text,
      RemoveOffset(predicted_state.selection, utf16_offset),
      RemoveOffset(range, utf16_offset), ui_ime_text_spans);
  surrounding_text_tracker_.OnSetCompositionFromExistingText(range);
  return true;
}

gfx::Range TextInput::GetAutocorrectRange() const {
  return autocorrect_info_.range;
}

gfx::Rect TextInput::GetAutocorrectCharacterBounds() const {
  return autocorrect_info_.bounds;
}

bool TextInput::SetAutocorrectRange(const gfx::Range& range) {
  if (range.is_empty()) {
    delegate_->SetAutocorrectRange(u"", range);
    return true;
  }

  const auto& predicted_state = surrounding_text_tracker_.predicted_state();
  if (!range.IsBoundedBy(predicted_state.GetSurroundingTextRange())) {
    return false;
  }

  delegate_->SetAutocorrectRange(
      predicted_state.surrounding_text,
      RemoveOffset(range, predicted_state.utf16_offset));
  return true;
}

std::optional<ui::GrammarFragment> TextInput::GetGrammarFragmentAtCursor()
    const {
  return grammar_fragment_at_cursor_;
}

bool TextInput::ClearGrammarFragments(const gfx::Range& range) {
  const auto& predicted_state = surrounding_text_tracker_.predicted_state();
  if (!range.IsBoundedBy(predicted_state.GetSurroundingTextRange())) {
    return false;
  }

  delegate_->ClearGrammarFragments(
      predicted_state.surrounding_text,
      RemoveOffset(range, predicted_state.utf16_offset));
  return true;
}

bool TextInput::AddGrammarFragments(
    const std::vector<ui::GrammarFragment>& fragments) {
  const auto& predicted_state = surrounding_text_tracker_.predicted_state();
  const gfx::Range surrounding_text_range =
      predicted_state.GetSurroundingTextRange();

  for (const auto& fragment : fragments) {
    if (!fragment.range.IsBoundedBy(surrounding_text_range)) {
      continue;
    }

    delegate_->AddGrammarFragment(
        predicted_state.surrounding_text,
        ui::GrammarFragment(
            RemoveOffset(fragment.range, predicted_state.utf16_offset),
            fragment.suggestion));
  }
  return true;
}

bool TextInput::SupportsAlwaysConfirmComposition() {
  return delegate_->SupportsConfirmPreedit();
}

void GetActiveTextInputControlLayoutBounds(
    std::optional<gfx::Rect>* control_bounds,
    std::optional<gfx::Rect>* selection_bounds) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void TextInput::OnKeyboardVisible(const gfx::Rect& keyboard_rect) {
  if (ShouldStageVKState()) {
    staged_vk_visible_ = true;
    // Bounds are now stale, so clear it.
    staged_vk_occluded_bounds_.reset();
    return;
  }
  delegate_->OnVirtualKeyboardVisibilityChanged(true);
}

void TextInput::OnKeyboardHidden() {
  if (ShouldStageVKState()) {
    staged_vk_occluded_bounds_ = gfx::Rect();
    staged_vk_visible_ = false;
    return;
  }
  delegate_->OnVirtualKeyboardOccludedBoundsChanged({});
  delegate_->OnVirtualKeyboardVisibilityChanged(false);
}

// This is called when the user switches input method.
void TextInput::InputMethodChanged(
    ash::input_method::InputMethodManager* manager,
    Profile* profile,
    bool show_message) {
  ui::TextInputType old_input_type = GetTextInputType();
  use_null_input_type_ = ShouldUseNullInputType(surrounding_text_supported_);
  if (input_method_ && GetTextInputType() != old_input_type) {
    input_method_->OnTextInputTypeChanged(this);
  }
}

void TextInput::OnSurfaceFocused(Surface* gained_focus,
                                 Surface* lost_focus,
                                 bool has_focused_surface) {
  DCHECK(surface_);
  if (gained_focus == lost_focus)
    return;

  if (gained_focus == surface_) {
    AttachInputMethod();
  } else if (lost_focus == surface_) {
    Deactivate();
  }
}

void TextInput::AttachInputMethod() {
  DCHECK(!input_method_);
  DCHECK(surface_);
  input_method_ = GetInputMethod(surface_->window());
  if (!input_method_) {
    LOG(ERROR) << "input method not found";
    return;
  }

  input_mode_ = ui::TEXT_INPUT_MODE_TEXT;
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  if (auto* controller = input_method_->GetVirtualKeyboardController())
    virtual_keyboard_observation_.Observe(controller);
  input_method_->SetFocusedTextInputClient(this);
  delegate_->Activated();

  if (pending_vk_visible_) {
    input_method_->SetVirtualKeyboardVisibilityIfEnabled(true);
    pending_vk_visible_ = false;
  }
}

void TextInput::DetachInputMethod() {
  if (!input_method_)
    return;
  input_mode_ = ui::TEXT_INPUT_MODE_DEFAULT;
  input_type_ = ui::TEXT_INPUT_TYPE_NONE;
  input_method_->DetachTextInputClient(this);
  virtual_keyboard_observation_.Reset();
  input_method_ = nullptr;
  delegate_->Deactivated();
}

bool TextInput::ShouldStageVKState() {
  return delegate_->SupportsFinalizeVirtualKeyboardChanges() &&
         pending_vk_finalize_;
}

void TextInput::SendStagedVKVisibility() {
  if (staged_vk_visible_) {
    delegate_->OnVirtualKeyboardVisibilityChanged(*staged_vk_visible_);
    staged_vk_visible_.reset();
  }
}

void TextInput::SendStagedVKOccludedBounds() {
  if (staged_vk_occluded_bounds_) {
    delegate_->OnVirtualKeyboardOccludedBoundsChanged(
        *staged_vk_occluded_bounds_);
    staged_vk_occluded_bounds_.reset();
  }
}

}  // namespace exo
