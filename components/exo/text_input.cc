// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/text_input.h"

#include <algorithm>

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "base/strings/utf_string_conversions.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method.h"
#include "ui/events/event.h"

namespace exo {

namespace {

ui::InputMethod* GetInputMethod(aura::Window* window) {
  if (!window || !window->GetHost())
    return nullptr;
  return window->GetHost()->GetInputMethod();
}

}  // namespace

size_t OffsetFromUTF8Offset(const base::StringPiece& text, uint32_t offset) {
  return base::UTF8ToUTF16(text.substr(0, offset)).size();
}

size_t OffsetFromUTF16Offset(const base::StringPiece16& text, uint32_t offset) {
  return base::UTF16ToUTF8(text.substr(0, offset)).size();
}

TextInput::TextInput(std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {}

TextInput::~TextInput() {
  if (keyboard_ui_controller_)
    keyboard_ui_controller_->RemoveObserver(this);
  if (input_method_)
    Deactivate();
}

void TextInput::Activate(Surface* surface) {
  DLOG_IF(ERROR, window_) << "Already activated with " << window_;
  DCHECK(surface);

  window_ = surface->window();
  AttachInputMethod();
}

void TextInput::Deactivate() {
  DetachInputMethod();
  window_ = nullptr;
}

void TextInput::ShowVirtualKeyboardIfEnabled() {
  // Some clients may ask showing virtual keyboard before sending activation.
  if (!input_method_) {
    pending_vk_visible_ = true;
    return;
  }
  input_method_->ShowVirtualKeyboardIfEnabled();
}

void TextInput::HideVirtualKeyboard() {
  if (keyboard_ui_controller_)
    keyboard_ui_controller_->HideKeyboardByUser();
  pending_vk_visible_ = false;
}

void TextInput::Resync() {
  if (input_method_)
    input_method_->OnCaretBoundsChanged(this);
}

void TextInput::SetSurroundingText(const base::string16& text,
                                   uint32_t cursor_pos,
                                   uint32_t anchor) {
  surrounding_text_ = text;
  cursor_pos_ = gfx::Range(cursor_pos);
  if (anchor < cursor_pos)
    cursor_pos_->set_start(anchor);
  else
    cursor_pos_->set_end(anchor);
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
  delegate_->SetCompositionText(composition);
}

void TextInput::ConfirmCompositionText(bool keep_selection) {
  // TODO(b/134473433) Modify this function so that when keep_selection is
  // true, the selection is not changed when text committed
  if (keep_selection) {
    NOTIMPLEMENTED_LOG_ONCE();
  }
  delegate_->Commit(composition_.text);
}

void TextInput::ClearCompositionText() {
  composition_ = ui::CompositionText();
  delegate_->SetCompositionText(composition_);
}

void TextInput::InsertText(const base::string16& text) {
  delegate_->Commit(text);
}

void TextInput::InsertChar(const ui::KeyEvent& event) {
  base::char16 ch = event.GetCharacter();
  if (u_isprint(ch)) {
    InsertText(base::string16(1, ch));
    return;
  }
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
  if (!cursor_pos_)
    return false;
  range->set_start(0);
  if (composition_.text.empty()) {
    range->set_end(surrounding_text_.size());
  } else {
    range->set_end(surrounding_text_.size() - cursor_pos_->length() +
                   composition_.text.size());
  }
  return true;
}

bool TextInput::GetCompositionTextRange(gfx::Range* range) const {
  if (!cursor_pos_ || composition_.text.empty())
    return false;

  range->set_start(cursor_pos_->start());
  range->set_end(cursor_pos_->start() + composition_.text.size());
  return true;
}

bool TextInput::GetEditableSelectionRange(gfx::Range* range) const {
  if (!cursor_pos_)
    return false;
  range->set_start(cursor_pos_->start());
  range->set_end(cursor_pos_->end());
  return true;
}

bool TextInput::SetEditableSelectionRange(const gfx::Range& range) {
  if (surrounding_text_.size() < range.GetMax())
    return false;
  delegate_->SetCursor(
      gfx::Range(OffsetFromUTF16Offset(surrounding_text_, range.start()),
                 OffsetFromUTF16Offset(surrounding_text_, range.end())));
  return true;
}

bool TextInput::DeleteRange(const gfx::Range& range) {
  if (surrounding_text_.size() < range.GetMax())
    return false;
  delegate_->DeleteSurroundingText(
      gfx::Range(OffsetFromUTF16Offset(surrounding_text_, range.start()),
                 OffsetFromUTF16Offset(surrounding_text_, range.end())));
  return true;
}

bool TextInput::GetTextFromRange(const gfx::Range& range,
                                 base::string16* text) const {
  gfx::Range text_range;
  if (!GetTextRange(&text_range) || !text_range.Contains(range))
    return false;
  if (composition_.text.empty() || range.GetMax() <= cursor_pos_->GetMin()) {
    text->assign(surrounding_text_, range.GetMin(), range.length());
    return true;
  }
  size_t composition_end = cursor_pos_->GetMin() + composition_.text.size();
  if (range.GetMin() >= composition_end) {
    size_t start =
        range.GetMin() - composition_.text.size() + cursor_pos_->length();
    text->assign(surrounding_text_, start, range.length());
    return true;
  }

  size_t start_in_composition = 0;
  if (range.GetMin() <= cursor_pos_->GetMin()) {
    text->assign(surrounding_text_, range.GetMin(),
                 cursor_pos_->GetMin() - range.GetMin());
  } else {
    start_in_composition = range.GetMin() - cursor_pos_->GetMin();
  }
  if (range.GetMax() <= composition_end) {
    text->append(composition_.text, start_in_composition,
                 range.GetMax() - cursor_pos_->GetMin() - start_in_composition);
  } else {
    text->append(composition_.text, start_in_composition,
                 composition_.text.size() - start_in_composition);
    text->append(surrounding_text_, cursor_pos_->GetMax(),
                 range.GetMax() - composition_end);
  }
  return true;
}

void TextInput::OnInputMethodChanged() {
  ui::InputMethod* input_method = GetInputMethod(window_);
  if (input_method == input_method_)
    return;
  input_method_->DetachTextInputClient(this);
  input_method_ = input_method;
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
  if (!cursor_pos_)
    return;
  uint32_t start =
      (cursor_pos_->GetMin() < before) ? 0 : (cursor_pos_->GetMin() - before);
  uint32_t end =
      std::min(cursor_pos_->GetMax() + after, surrounding_text_.size());
  delegate_->DeleteSurroundingText(
      gfx::Range(OffsetFromUTF16Offset(surrounding_text_, start),
                 OffsetFromUTF16Offset(surrounding_text_, end)));
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
  // TODO(https://crbug.com/952757): Implement this method.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void TextInput::OnKeyboardVisibilityChanged(bool is_visible) {
  delegate_->OnVirtualKeyboardVisibilityChanged(is_visible);
}

void TextInput::AttachInputMethod() {
  DCHECK(!input_method_);

  ui::InputMethod* input_method = GetInputMethod(window_);
  if (!input_method) {
    LOG(ERROR) << "input method not found";
    return;
  }

  input_mode_ = ui::TEXT_INPUT_MODE_TEXT;
  input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  input_method_ = input_method;
  input_method_->SetFocusedTextInputClient(this);
  delegate_->Activated();

  if (!keyboard_ui_controller_ &&
      keyboard::KeyboardUIController::HasInstance()) {
    auto* keyboard_ui_controller = keyboard::KeyboardUIController::Get();
    if (keyboard_ui_controller->IsEnabled()) {
      keyboard_ui_controller_ = keyboard_ui_controller;
      keyboard_ui_controller_->AddObserver(this);
    }
  }

  if (pending_vk_visible_) {
    input_method_->ShowVirtualKeyboardIfEnabled();
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
  input_method_ = nullptr;
  delegate_->Deactivated();
}

}  // namespace exo
