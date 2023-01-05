// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/text_input.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
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

}  // namespace

TextInput::TextInput(std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {}

TextInput::~TextInput() {
  Deactivate();
}

void TextInput::Activate(Seat* seat, Surface* surface) {
  DCHECK(surface);
  DCHECK(seat);
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
  surrounding_text_tracker_.Reset();
  if (input_method_)
    input_method_->CancelComposition(this);
}

void TextInput::SetSurroundingText(const std::u16string& text,
                                   const gfx::Range& cursor_pos) {
  surrounding_text_tracker_.Update(text, cursor_pos);

  // Convert utf8 grammar fragment to utf16.
  if (grammar_fragment_at_cursor_utf8_) {
    std::string utf8_text = base::UTF16ToUTF8(text);
    ui::GrammarFragment fragment = *grammar_fragment_at_cursor_utf8_;
    std::vector<size_t> offsets = {fragment.range.start(),
                                   fragment.range.end()};
    base::UTF8ToUTF16AndAdjustOffsets(utf8_text, &offsets);
    grammar_fragment_at_cursor_utf16_ = ui::GrammarFragment(
        gfx::Range(offsets[0], offsets[1]), fragment.suggestion);
  } else {
    grammar_fragment_at_cursor_utf16_ = absl::nullopt;
  }

  if (pending_autocorrect_info_) {
    autocorrect_info_ = *pending_autocorrect_info_;
    pending_autocorrect_info_ = absl::nullopt;
  }

  // TODO(b/206068262): Consider introducing an API to notify surrounding text
  // update explicitly.
  if (input_method_)
    input_method_->OnCaretBoundsChanged(this);
}

void TextInput::SetTypeModeFlags(ui::TextInputType type,
                                 ui::TextInputMode mode,
                                 int flags,
                                 bool should_do_learning) {
  if (!input_method_)
    return;
  bool changed = (input_type_ != type) || (input_mode_ != mode) ||
                 (flags_ != flags) ||
                 (should_do_learning_ != should_do_learning);
  input_type_ = type;
  input_mode_ = mode;
  flags_ = flags;
  should_do_learning_ = should_do_learning;
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

void TextInput::SetGrammarFragmentAtCursor(
    const absl::optional<ui::GrammarFragment>& fragment) {
  grammar_fragment_at_cursor_utf16_ = absl::nullopt;
  grammar_fragment_at_cursor_utf8_ = fragment;
}

void TextInput::SetAutocorrectInfo(const gfx::Range& autocorrect_range,
                                   const gfx::Rect& autocorrect_bounds) {
  // Since we receive the autocorrect information separately from the
  // surrounding text information, the range and bounds may be invalid at this
  // point, because the surrounding text this class holds is stale.
  // Save it as the "pending" information a surrounding text update is received.
  pending_autocorrect_info_ = {autocorrect_range, autocorrect_bounds};
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

void TextInput::SetCompositionText(const ui::CompositionText& composition) {
  delegate_->SetCompositionText(composition);
  surrounding_text_tracker_.OnSetCompositionText(composition);
}

size_t TextInput::ConfirmCompositionText(bool keep_selection) {
  const auto& [surrounding_text, cursor_pos, composition] =
      surrounding_text_tracker_.predicted_state();

  const size_t composition_text_length = composition.length();
  if (keep_selection && cursor_pos.IsValid() &&
      surrounding_text.length() >= cursor_pos.GetMax()) {
    delegate_->SetCursor(surrounding_text, cursor_pos);
  }
  base::StringPiece16 composition_text =
      composition.is_empty()
          ? base::StringPiece16()
          : base::StringPiece16(surrounding_text)
                .substr(composition.GetMin(), composition.length());

  delegate_->Commit(composition_text);
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
  // TODO(b/240618514): Short term workaround to accept temporary fix in IME
  // for urgent production breakage.
  // We should come up with the proper solution of what to be done.
  if (event.key_code() == ui::VKEY_UNKNOWN) {
    // On some specific cases, IME use InsertChar, even if there's no clear
    // key mapping from key_code. Then, use InsertText().
    InsertText(std::u16string(1u, event.GetCharacter()),
               InsertTextCursorBehavior::kMoveCursorAfterText);
    return;
  }
  // TextInput is currently used only for Lacros, and this is the
  // short term workaround not to duplicate KeyEvent there.
  // This is what we do for ARC, which is being removed in the near
  // future.
  // TODO(fukino): Get rid of this, too, when the wl_keyboard::key
  // and text_input::keysym events are handled properly in Lacros.
  if (ConsumedByIme(surface_->window(), event))
    delegate_->SendKey(event);
}

ui::TextInputType TextInput::GetTextInputType() const {
  return input_type_;
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
  return true;
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
  NOTIMPLEMENTED_LOG_ONCE();
  return ui::TextInputClient::FOCUS_REASON_OTHER;
}

bool TextInput::GetTextRange(gfx::Range* range) const {
  DCHECK(range);
  const auto& [surrounding_text, selection, unused_composition] =
      surrounding_text_tracker_.predicted_state();
  DCHECK(selection.IsValid());

  *range = gfx::Range(0, surrounding_text.length());
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
  const auto& [surrounding_text, unused_selection, composition] =
      surrounding_text_tracker_.predicted_state();
  if (surrounding_text.length() < range.GetMax())
    return false;

  // Send a SetCursor followed by a Commit of the current composition text, or
  // empty string if there is no composition text. This is necessary since
  // SetCursor only takes effect on the following Commit.
  delegate_->SetCursor(surrounding_text, range);
  delegate_->Commit(base::StringPiece16(surrounding_text)
                        .substr(composition.GetMin(), composition.length()));
  surrounding_text_tracker_.OnSetEditableSelectionRange(range);
  return true;
}

bool TextInput::GetTextFromRange(const gfx::Range& range,
                                 std::u16string* text) const {
  DCHECK(text);
  const auto& surrounding_text =
      surrounding_text_tracker_.predicted_state().surrounding_text;
  if (surrounding_text.length() < range.GetMax())
    return false;
  text->assign(surrounding_text, range.GetMin(), range.length());
  return true;
}

void TextInput::OnInputMethodChanged() {
  DCHECK_EQ(surface_, seat_->GetFocusedSurface());
  ui::InputMethod* input_method = GetInputMethod(surface_->window());
  if (input_method == input_method_)
    return;
  input_method_->DetachTextInputClient(this);
  virtual_keyboard_observation_.Reset();
  input_method_ = input_method;
  if (auto* controller = input_method_->GetVirtualKeyboardController())
    virtual_keyboard_observation_.Observe(controller);
  input_method_->SetFocusedTextInputClient(this);
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
  const auto& [surrounding_text, selection, unused_composition] =
      surrounding_text_tracker_.predicted_state();

  DCHECK(selection.IsValid());

  size_t utf16_start =
      selection.GetMin() - std::min(before, selection.GetMin());
  size_t utf16_end =
      std::min(selection.GetMax() + after, surrounding_text.length());
  delegate_->DeleteSurroundingText(surrounding_text,
                                   gfx::Range(utf16_start, utf16_end));
  surrounding_text_tracker_.OnExtendSelectionAndDelete(before, after);
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
  const auto& [surrounding_text, selection, unused_composition] =
      surrounding_text_tracker_.predicted_state();
  DCHECK(selection.IsValid());
  if (surrounding_text.length() < range.GetMax())
    return false;

  const auto composition_length = range.length();
  for (const auto& span : ui_ime_text_spans) {
    if (composition_length < std::max(span.start_offset, span.end_offset))
      return false;
  }

  delegate_->SetCompositionFromExistingText(surrounding_text, selection, range,
                                            ui_ime_text_spans);
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
  const auto& surrounding_text =
      surrounding_text_tracker_.predicted_state().surrounding_text;
  delegate_->SetAutocorrectRange(surrounding_text, range);
  return true;
}

absl::optional<ui::GrammarFragment> TextInput::GetGrammarFragmentAtCursor()
    const {
  return grammar_fragment_at_cursor_utf16_;
}

bool TextInput::ClearGrammarFragments(const gfx::Range& range) {
  const auto& surrounding_text =
      surrounding_text_tracker_.predicted_state().surrounding_text;
  if (surrounding_text.length() < range.GetMax())
    return false;

  delegate_->ClearGrammarFragments(surrounding_text, range);
  return true;
}

bool TextInput::AddGrammarFragments(
    const std::vector<ui::GrammarFragment>& fragments) {
  const auto& surrounding_text =
      surrounding_text_tracker_.predicted_state().surrounding_text;
  for (auto& fragment : fragments) {
    if (surrounding_text.length() < fragment.range.GetMax())
      continue;
    delegate_->AddGrammarFragment(surrounding_text, fragment);
  }
  return true;
}

void GetActiveTextInputControlLayoutBounds(
    absl::optional<gfx::Rect>* control_bounds,
    absl::optional<gfx::Rect>* selection_bounds) {
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
