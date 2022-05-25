// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/text_input.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
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

ui::InputMethod* GetInputMethod(aura::Window* window) {
  if (!window || !window->GetHost())
    return nullptr;
  return window->GetHost()->GetInputMethod();
}

ui::CompositionText GenerateCompositionTextFrom(
    const std::u16string& surrounding_text,
    const gfx::Range& composition_range,
    const std::vector<ui::ImeTextSpan>& ui_ime_text_spans) {
  ui::CompositionText composition;
  composition.text = surrounding_text.substr(composition_range.GetMin(),
                                             composition_range.length());
  composition.ime_text_spans = ui_ime_text_spans;
  return composition;
}

}  // namespace

TextInput::TextInput(std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {}

TextInput::~TextInput() {
  if (input_method_)
    Deactivate();
}

void TextInput::Activate(Surface* surface) {
  DCHECK(surface);
  AttachInputMethod(surface->window());
}

void TextInput::Deactivate() {
  DetachInputMethod();
}

void TextInput::ShowVirtualKeyboardIfEnabled() {
  // Some clients may ask showing virtual keyboard before sending activation.
  if (!input_method_) {
    pending_vk_visible_ = true;
    return;
  }
  input_method_->SetVirtualKeyboardVisibilityIfEnabled(true);
}

void TextInput::HideVirtualKeyboard() {
  if (input_method_)
    input_method_->SetVirtualKeyboardVisibilityIfEnabled(false);
  pending_vk_visible_ = false;
}

void TextInput::Resync() {
  if (input_method_)
    input_method_->OnCaretBoundsChanged(this);
}

void TextInput::Reset() {
  ResetCompositionTextCache();
  if (input_method_)
    input_method_->CancelComposition(this);
}

void TextInput::SetSurroundingText(const std::u16string& text,
                                   const gfx::Range& cursor_pos) {
  surrounding_text_ = text;
  cursor_pos_ = cursor_pos;

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
  bool changed = (input_type_ != type);
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

void TextInput::SetCompositionText(const ui::CompositionText& composition) {
  composition_ = composition;
  // Identify the starting index of the current composition. If a composition
  // range has been set previously, then use it's starting index, otherwise
  // use the current cursor position as the start of the composition. If the
  // user has a selection, then we can assume the min value of the cursor_pos
  // range as the start of the composition, as the selection will be replaced
  // by the composition text being set.
  uint32_t composition_start = composition_range_.IsValid()
                                   ? composition_range_.GetMin()
                                   : cursor_pos_.GetMin();
  composition_range_ = gfx::Range(composition_start,
                                  composition_start + composition.text.size());
  delegate_->SetCompositionText(composition);
}

uint32_t TextInput::ConfirmCompositionText(bool keep_selection) {
  // TODO(b/134473433) Modify this function so that when keep_selection is
  // true, the selection is not changed when text committed
  if (keep_selection) {
    NOTIMPLEMENTED_LOG_ONCE();
  }
  const uint32_t composition_text_length =
      static_cast<uint32_t>(composition_.text.length());
  delegate_->Commit(composition_.text);
  ResetCompositionTextCache();
  return composition_text_length;
}

void TextInput::ClearCompositionText() {
  if (composition_.text.empty())
    return;
  ResetCompositionTextCache();
  delegate_->SetCompositionText(composition_);
}

void TextInput::InsertText(const std::u16string& text,
                           InsertTextCursorBehavior cursor_behavior) {
  // TODO(crbug.com/1155331): Handle |cursor_behavior| correctly.
  delegate_->Commit(text);
  ResetCompositionTextCache();
}

void TextInput::InsertChar(const ui::KeyEvent& event) {
  // TextInput is currently used only for Lacros, and this is the
  // short term workaround not to duplicate KeyEvent there.
  // This is what we do for ARC, which is being removed in the near
  // future.
  // TODO(fukino): Get rid of this, too, when the wl_keyboard::key
  // and text_input::keysym events are handled properly in Lacros.
  if (window_ && ConsumedByIme(window_, event))
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
  return caret_bounds_ + window_->GetBoundsInScreen().OffsetFromOrigin();
}

gfx::Rect TextInput::GetSelectionBoundingBox() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

bool TextInput::GetCompositionCharacterBounds(uint32_t index,
                                              gfx::Rect* rect) const {
  return false;
}

bool TextInput::HasCompositionText() const {
  return !composition_.text.empty();
}

ui::TextInputClient::FocusReason TextInput::GetFocusReason() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return ui::TextInputClient::FOCUS_REASON_OTHER;
}

bool TextInput::GetTextRange(gfx::Range* range) const {
  if (!cursor_pos_.IsValid())
    return false;
  range->set_start(0);
  range->set_end(surrounding_text_.size());
  return true;
}

bool TextInput::GetCompositionTextRange(gfx::Range* range) const {
  DCHECK(range);
  if (composition_range_.IsValid()) {
    *range = composition_range_;
    return true;
  }

  return false;
}

bool TextInput::GetEditableSelectionRange(gfx::Range* range) const {
  if (!cursor_pos_.IsValid())
    return false;
  range->set_start(cursor_pos_.start());
  range->set_end(cursor_pos_.end());
  return true;
}

bool TextInput::SetEditableSelectionRange(const gfx::Range& range) {
  if (surrounding_text_.size() < range.GetMax())
    return false;
  delegate_->SetCursor(surrounding_text_, range);
  return true;
}

bool TextInput::DeleteRange(const gfx::Range& range) {
  if (surrounding_text_.size() < range.GetMax())
    return false;
  delegate_->DeleteSurroundingText(surrounding_text_, range);
  return true;
}

bool TextInput::GetTextFromRange(const gfx::Range& range,
                                 std::u16string* text) const {
  gfx::Range text_range;
  if (!GetTextRange(&text_range) || !text_range.Contains(range))
    return false;
  text->assign(surrounding_text_.substr(range.GetMin(), range.length()));
  return true;
}

void TextInput::OnInputMethodChanged() {
  ui::InputMethod* input_method = GetInputMethod(window_);
  if (input_method == input_method_)
    return;
  input_method_->DetachTextInputClient(this);
  if (auto* controller = input_method_->GetVirtualKeyboardController())
    controller->RemoveObserver(this);
  input_method_ = input_method;
  if (auto* controller = input_method_->GetVirtualKeyboardController())
    controller->AddObserver(this);
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
  if (!cursor_pos_.IsValid())
    return;
  size_t utf16_start =
      std::max(static_cast<size_t>(cursor_pos_.GetMin()), before) - before;
  size_t utf16_end =
      std::min(cursor_pos_.GetMax() + after, surrounding_text_.size());
  delegate_->DeleteSurroundingText(surrounding_text_,
                                   gfx::Range(utf16_start, utf16_end));
}

void TextInput::EnsureCaretNotInRect(const gfx::Rect& rect) {}

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
  if (!cursor_pos_.IsValid())
    return false;
  if (surrounding_text_.size() < range.GetMax())
    return false;

  const auto composition_length = range.length();
  for (const auto& span : ui_ime_text_spans) {
    if (composition_length < std::max(span.start_offset, span.end_offset))
      return false;
  }

  composition_ =
      GenerateCompositionTextFrom(surrounding_text_, range, ui_ime_text_spans);
  composition_range_.set_start(range.GetMin());
  composition_range_.set_end(range.GetMax());
  delegate_->SetCompositionFromExistingText(surrounding_text_, cursor_pos_,
                                            range, ui_ime_text_spans);
  return true;
}

gfx::Range TextInput::GetAutocorrectRange() const {
  // TODO(https://crbug.com/952757): Implement this method.
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::Range();
}

gfx::Rect TextInput::GetAutocorrectCharacterBounds() const {
  // TODO(https://crbug.com/952757): Implement this method.
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::Rect();
}

// TODO(crbug.com/1091088) Implement setAutocorrectRange
bool TextInput::SetAutocorrectRange(const gfx::Range& range) {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

absl::optional<ui::GrammarFragment> TextInput::GetGrammarFragmentAtCursor()
    const {
  // TODO(https://crbug.com/1201454): Implement this method.
  NOTIMPLEMENTED_LOG_ONCE();
  return absl::nullopt;
}

bool TextInput::ClearGrammarFragments(const gfx::Range& range) {
  // TODO(https://crbug.com/1201454): Implement this method.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool TextInput::AddGrammarFragments(
    const std::vector<ui::GrammarFragment>& fragments) {
  // TODO(https://crbug.com/1201454): Implement this method.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void GetActiveTextInputControlLayoutBounds(
    absl::optional<gfx::Rect>* control_bounds,
    absl::optional<gfx::Rect>* selection_bounds) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void TextInput::OnKeyboardVisible(const gfx::Rect& keyboard_rect) {
  delegate_->OnVirtualKeyboardVisibilityChanged(true);
}

void TextInput::OnKeyboardHidden() {
  delegate_->OnVirtualKeyboardVisibilityChanged(false);
}

void TextInput::AttachInputMethod(aura::Window* window) {
  DCHECK(window);

  if (window_) {
    if (window == window_)
      return;
    DetachInputMethod();
  }
  DCHECK(!input_method_);

  window_ = window;
  input_method_ = GetInputMethod(window_);
  if (!input_method_) {
    LOG(ERROR) << "input method not found";
    return;
  }

  input_mode_ = ui::TEXT_INPUT_MODE_TEXT;
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  if (auto* controller = input_method_->GetVirtualKeyboardController())
    controller->AddObserver(this);
  input_method_->SetFocusedTextInputClient(this);
  delegate_->Activated();

  if (pending_vk_visible_) {
    input_method_->SetVirtualKeyboardVisibilityIfEnabled(true);
    pending_vk_visible_ = false;
  }
}

void TextInput::DetachInputMethod() {
  if (!input_method_) {
    DLOG(ERROR) << "input method already detached";
    return;
  }
  input_mode_ = ui::TEXT_INPUT_MODE_DEFAULT;
  input_type_ = ui::TEXT_INPUT_TYPE_NONE;
  input_method_->DetachTextInputClient(this);
  if (auto* controller = input_method_->GetVirtualKeyboardController())
    controller->RemoveObserver(this);
  input_method_ = nullptr;
  window_ = nullptr;
  delegate_->Deactivated();
}

void TextInput::ResetCompositionTextCache() {
  composition_ = ui::CompositionText();
  composition_range_ = gfx::Range::InvalidRange();
}

}  // namespace exo
