// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_EMOJI_EMOJI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_EMOJI_EMOJI_HANDLER_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace chromeos {

// The WebUI message handler for chrome://emoji-picker
class EmojiHandler : public content::WebUIMessageHandler {
 public:
  EmojiHandler();
  ~EmojiHandler() override;

 private:
  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  void HandleInsertEmoji(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(EmojiHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_EMOJI_EMOJI_HANDLER_H_
