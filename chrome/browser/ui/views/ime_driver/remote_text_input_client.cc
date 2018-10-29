// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ime_driver/remote_text_input_client.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "ui/events/event_dispatcher.h"

RemoteTextInputClient::RemoteTextInputClient(
    ws::mojom::TextInputClientPtr remote_client,
    ui::TextInputType text_input_type,
    ui::TextInputMode text_input_mode,
    base::i18n::TextDirection text_direction,
    int text_input_flags,
    gfx::Rect caret_bounds)
    : remote_client_(std::move(remote_client)),
      text_input_type_(text_input_type),
      text_input_mode_(text_input_mode),
      text_direction_(text_direction),
      text_input_flags_(text_input_flags),
      caret_bounds_(caret_bounds) {}

RemoteTextInputClient::~RemoteTextInputClient() {
  while (!pending_callbacks_.empty()) {
    auto callback = std::move(pending_callbacks_.front());
    pending_callbacks_.pop();
    std::move(callback).Run(false);
  }
}

void RemoteTextInputClient::SetTextInputType(
    ui::TextInputType text_input_type) {
  text_input_type_ = text_input_type;
}

void RemoteTextInputClient::SetCaretBounds(const gfx::Rect& caret_bounds) {
  caret_bounds_ = caret_bounds;
}

void RemoteTextInputClient::OnDispatchKeyEventPostIMECompleted(bool completed) {
  DCHECK(!pending_callbacks_.empty());
  base::OnceCallback<void(bool)> callback =
      std::move(pending_callbacks_.front());
  pending_callbacks_.pop();
  if (callback)
    std::move(callback).Run(completed);
}

void RemoteTextInputClient::SetCompositionText(
    const ui::CompositionText& composition) {
  remote_client_->SetCompositionText(composition);
}

void RemoteTextInputClient::ConfirmCompositionText() {
  remote_client_->ConfirmCompositionText();
}

void RemoteTextInputClient::ClearCompositionText() {
  remote_client_->ClearCompositionText();
}

void RemoteTextInputClient::InsertText(const base::string16& text) {
  remote_client_->InsertText(text);
}

void RemoteTextInputClient::InsertChar(const ui::KeyEvent& event) {
  remote_client_->InsertChar(ui::Event::Clone(event));
}

ui::TextInputType RemoteTextInputClient::GetTextInputType() const {
  return text_input_type_;
}

ui::TextInputMode RemoteTextInputClient::GetTextInputMode() const {
  return text_input_mode_;
}

base::i18n::TextDirection RemoteTextInputClient::GetTextDirection() const {
  return text_direction_;
}

int RemoteTextInputClient::GetTextInputFlags() const {
  return text_input_flags_;
}

bool RemoteTextInputClient::CanComposeInline() const {
  // If we return false here, ui::InputMethodChromeOS will try to create a
  // composition window. But here we are at IMEDriver, and composition
  // window shouldn't be created by IMEDriver.
  return true;
}

gfx::Rect RemoteTextInputClient::GetCaretBounds() const {
  return caret_bounds_;
}

bool RemoteTextInputClient::GetCompositionCharacterBounds(
    uint32_t index,
    gfx::Rect* rect) const {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool RemoteTextInputClient::HasCompositionText() const {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

ui::TextInputClient::FocusReason RemoteTextInputClient::GetFocusReason() const {
  // TODO(https://crbug.com/824604): Implement this correctly.
  NOTIMPLEMENTED_LOG_ONCE();
  return ui::TextInputClient::FOCUS_REASON_OTHER;
}

bool RemoteTextInputClient::GetTextRange(gfx::Range* range) const {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool RemoteTextInputClient::GetCompositionTextRange(gfx::Range* range) const {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool RemoteTextInputClient::GetSelectionRange(gfx::Range* range) const {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool RemoteTextInputClient::SetSelectionRange(const gfx::Range& range) {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool RemoteTextInputClient::DeleteRange(const gfx::Range& range) {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool RemoteTextInputClient::GetTextFromRange(const gfx::Range& range,
                                             base::string16* text) const {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void RemoteTextInputClient::OnInputMethodChanged() {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED_LOG_ONCE();
}

bool RemoteTextInputClient::ChangeTextDirectionAndLayoutAlignment(
    base::i18n::TextDirection direction) {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void RemoteTextInputClient::ExtendSelectionAndDelete(size_t before,
                                                     size_t after) {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED_LOG_ONCE();
}

void RemoteTextInputClient::EnsureCaretNotInRect(const gfx::Rect& rect) {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED_LOG_ONCE();
}

bool RemoteTextInputClient::IsTextEditCommandEnabled(
    ui::TextEditCommand command) const {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void RemoteTextInputClient::SetTextEditCommandForNextKeyEvent(
    ui::TextEditCommand command) {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED_LOG_ONCE();
}

ukm::SourceId RemoteTextInputClient::GetClientSourceForMetrics() const {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED_LOG_ONCE();
  return ukm::SourceId();
}

bool RemoteTextInputClient::ShouldDoLearning() {
  // TODO(https://crbug.com/311180): Implement this method.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

ui::EventDispatchDetails RemoteTextInputClient::DispatchKeyEventPostIME(
    ui::KeyEvent* event,
    base::OnceCallback<void(bool)> ack_callback) {
  pending_callbacks_.push(std::move(ack_callback));
  remote_client_->DispatchKeyEventPostIME(
      ui::Event::Clone(*event),
      base::BindOnce(&RemoteTextInputClient::OnDispatchKeyEventPostIMECompleted,
                     weak_ptr_factory_.GetWeakPtr()));
  return ui::EventDispatchDetails();
}
