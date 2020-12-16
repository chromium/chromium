// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/emoji/emoji_picker.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/emoji_picker_resources.h"
#include "chrome/grit/emoji_picker_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {

EmojiPicker::EmojiPicker(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://emoji-picker source.
  std::unique_ptr<content::WebUIDataSource> html_source(
      content::WebUIDataSource::Create(chrome::kChromeUIEmojiPickerHost));

  // As a demonstration of passing a variable for JS to use we pass in some
  // emoji.
  html_source->AddString("emoji",
                         "😀,😃,😄,😁,😆,😅,😂,🤣,😭");
  html_source->UseStringsJs();

  // Add required resources.
  webui::SetupWebUIDataSource(
      html_source.get(),
      base::make_span(kEmojiPickerResources, kEmojiPickerResourcesSize),
      IDR_EMOJI_PICKER_INDEX_HTML);

  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, html_source.release());
}

EmojiPicker::~EmojiPicker() {}

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
