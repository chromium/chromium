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

namespace chromeos {

EmojiPickerDialog::EmojiPickerDialog() {}

void EmojiPickerDialog::Show() {
  chrome::ShowWebDialog(nullptr, ProfileManager::GetActiveUserProfile(),
                        new EmojiPickerDialog());
}

ui::ModalType EmojiPickerDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_NONE;
}

base::string16 EmojiPickerDialog::GetDialogTitle() const {
  return base::UTF8ToUTF16("Emoji picker");
}

GURL EmojiPickerDialog::GetDialogContentURL() const {
  return GURL(chrome::kChromeUIEmojiPickerURL);
}

void EmojiPickerDialog::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) const {}

void EmojiPickerDialog::GetDialogSize(gfx::Size* size) const {
  const int kDefaultWidth = 544;
  const int kDefaultHeight = 628;
  size->SetSize(kDefaultWidth, kDefaultHeight);
}

std::string EmojiPickerDialog::GetDialogArgs() const {
  return "";
}

void EmojiPickerDialog::OnDialogShown(content::WebUI* webui) {
  webui_ = webui;
}

void EmojiPickerDialog::OnDialogClosed(const std::string& json_retval) {
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
