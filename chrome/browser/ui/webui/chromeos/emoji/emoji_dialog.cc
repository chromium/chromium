// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/emoji/emoji_dialog.h"

#include "base/strings/utf_string_conversions.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/common/url_constants.h"

#include "ui/aura/window.h"
#include "ui/base/ime/chromeos/ime_bridge.h"

namespace chromeos {

constexpr gfx::Size kDefaultWindowSize(340, 390);

gfx::NativeWindow EmojiPickerDialog::window = nullptr;

EmojiPickerDialog::EmojiPickerDialog() {
  set_can_resize(false);
}

void EmojiPickerDialog::Show() {
  if (window) {
    window->Focus();
    return;
  }
  ui::InputMethod* input_method =
      ui::IMEBridge::Get()->GetInputContextHandler()->GetInputMethod();
  const ui::TextInputClient* input_client =
      input_method ? input_method->GetTextInputClient() : nullptr;
  const gfx::Rect caret_bounds =
      input_client ? input_client->GetCaretBounds() : gfx::Rect();
  window = chrome::ShowWebDialog(
      nullptr, ProfileManager::GetActiveUserProfile(), new EmojiPickerDialog());
  // For now, this can overflow the screen.
  if (input_client) {
    window->SetBounds(gfx::Rect(caret_bounds.x(), caret_bounds.bottom(),
                                kDefaultWindowSize.width(),
                                kDefaultWindowSize.height()));
  }
}

ui::ModalType EmojiPickerDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_NONE;
}

base::string16 EmojiPickerDialog::GetDialogTitle() const {
  return base::UTF8ToUTF16("Emoji Picker");
}

GURL EmojiPickerDialog::GetDialogContentURL() const {
  return GURL(chrome::kChromeUIEmojiPickerURL);
}

void EmojiPickerDialog::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) const {}

void EmojiPickerDialog::GetDialogSize(gfx::Size* size) const {
  *size = kDefaultWindowSize;
}

std::string EmojiPickerDialog::GetDialogArgs() const {
  return "";
}

void EmojiPickerDialog::OnDialogShown(content::WebUI* webui) {
  webui_ = webui;
}

void EmojiPickerDialog::OnDialogClosed(const std::string& json_retval) {
  window = nullptr;
  delete this;
}

void EmojiPickerDialog::OnCloseContents(content::WebContents* source,
                                        bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool EmojiPickerDialog::ShouldShowDialogTitle() const {
  return true;
}

EmojiPickerDialog::~EmojiPickerDialog() = default;
}  // namespace chromeos
