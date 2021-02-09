// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/emoji/emoji_picker.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/emoji_picker_resources.h"
#include "chrome/grit/emoji_picker_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/resources/grit/webui_generated_resources.h"

#include "chrome/browser/ui/webui/chromeos/emoji/emoji_dialog.h"
#include "chrome/browser/ui/webui/chromeos/emoji/emoji_handler.h"

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

  // Attach message handler to handle emoji click events.
  web_ui->AddMessageHandler(std::make_unique<EmojiHandler>());

  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, html_source.release());
}

EmojiPicker::~EmojiPicker() {}

}  // namespace chromeos
