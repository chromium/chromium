// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/emoji/emoji_page_handler.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ui/webui/chromeos/emoji/emoji_ui.h"
#include "ui/base/ime/chromeos/ime_bridge.h"

namespace chromeos {

// Keep in sync with entry in enums.xml.
enum class EmojiVariantType {
  // smaller entries only used by Chrome OS VK
  kEmojiPickerBase = 4,
  kEmojiPickerVariant = 5,
  kMaxValue = kEmojiPickerVariant,
};

void LogInsertEmoji(bool is_variant) {
  EmojiVariantType insert_value = is_variant
                                      ? EmojiVariantType::kEmojiPickerVariant
                                      : EmojiVariantType::kEmojiPickerBase;
  base::UmaHistogramEnumeration("InputMethod.SystemEmojiPicker.TriggerType",
                                insert_value);
}

EmojiPageHandler::EmojiPageHandler(
    mojo::PendingReceiver<emoji_picker::mojom::PageHandler> receiver,
    content::WebUI* web_ui,
    EmojiUI* webui_controller,
    bool incognito_mode)
    : receiver_(this, std::move(receiver)),
      webui_controller_(webui_controller),
      incognito_mode_(incognito_mode) {}

EmojiPageHandler::~EmojiPageHandler() {}

void EmojiPageHandler::ShowUI() {
  auto embedder = webui_controller_->embedder();
  // Embedder may not exist in some cases (e.g. user browses to
  // chrome://emoji-picker directly rather than using right click on
  // text field -> emoji).
  if (embedder) {
    embedder->ShowUI();
  }
}

void EmojiPageHandler::IsIncognitoTextField(
    IsIncognitoTextFieldCallback callback) {
  std::move(callback).Run(incognito_mode_);
}

void EmojiPageHandler::InsertEmoji(const std::string& emoji_to_insert,
                                   bool is_variant) {
  // By hiding the emoji picker, we restore focus to the original text field so
  // we can insert the text.
  auto embedder = webui_controller_->embedder();
  if (embedder) {
    embedder->CloseUI();
  }
  LogInsertEmoji(is_variant);

  // In theory, we are returning focus to the input field where the user
  // originally selected emoji. However, the input field may not exist anymore
  // e.g. JS has mutated the web page while emoji picker was open, so check that
  // a valid input client is available as part of inserting the emoji.
  ui::InputMethod* input_method =
      ui::IMEBridge::Get()->GetInputContextHandler()->GetInputMethod();
  if (!input_method) {
    DLOG(WARNING) << "no input_method found";
    return;
  }

  ui::TextInputClient* input_client = input_method->GetTextInputClient();

  if (!input_client) {
    DLOG(WARNING) << "no input_client found";
    return;
  }

  if (input_client->GetTextInputType() ==
      ui::TextInputType::TEXT_INPUT_TYPE_NONE) {
    DLOG(WARNING) << "attempt to insert into input_client with type none";
    return;
  }

  input_client->InsertText(
      base::UTF8ToUTF16(emoji_to_insert),
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
}

}  // namespace chromeos
