// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/text_input.h"

#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method.h"
#include "ui/events/event.h"
#include "ui/keyboard/keyboard_controller.h"

namespace exo {

namespace {

ui::InputMethod* GetInputMethod(aura::Window* window) {
  if (!window || !window->GetHost())
    return nullptr;
  return window->GetHost()->GetInputMethod();
}

}  // namespace

TextInput::TextInput(std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {}

TextInput::~TextInput() {
  if (keyboard_controller_)
    keyboard_controller_->RemoveObserver(this);
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
  if (keyboard_controller_)
    keyboard_controller_->HideKeyboardByUser();
  pending_vk_visible_ = false;
}

void TextInput::Resync() {
  if (input_method_)
    input_method_->OnCaretBoundsChanged(this);
}

void TextInput::SetSurroundingText(const base::string16& text,
                                   uint32_t cursor_pos) {
  NOTIMPLEMENTED();
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

void TextInput::ConfirmCompositionText() {
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
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool TextInput::GetCompositionTextRange(gfx::Range* range) const {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool TextInput::GetSelectionRange(gfx::Range* range) const {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool TextInput::SetSelectionRange(const gfx::Range& range) {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool TextInput::DeleteRange(const gfx::Range& range) {
  // TODO(mukai): call delegate_->DeleteSurroundingText(range) once it's
  // supported.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool TextInput::GetTextFromRange(const gfx::Range& range,
                                 base::string16* text) const {
  // TODO(mukai): support of surrounding text.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
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

void TextInput::ExtendSelectionAndDelete(size_t before, size_t after) {}

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

void TextInput::OnKeyboardVisibilityStateChanged(bool is_visible) {
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

  if (!keyboard_controller_ && keyboard::KeyboardController::HasInstance()) {
    auto* keyboard_controller = keyboard::KeyboardController::Get();
    if (keyboard_controller->IsEnabled()) {
      keyboard_controller_ = keyboard_controller;
      keyboard_controller_->AddObserver(this);
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
