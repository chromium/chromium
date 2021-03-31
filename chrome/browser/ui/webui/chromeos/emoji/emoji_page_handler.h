// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_EMOJI_EMOJI_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_EMOJI_EMOJI_PAGE_HANDLER_H_

#include "chrome/browser/ui/webui/chromeos/emoji/emoji_picker.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"

namespace chromeos {
class EmojiUI;

class EmojiPageHandler : public emoji_picker::mojom::PageHandler {
 public:
  EmojiPageHandler(
      mojo::PendingReceiver<emoji_picker::mojom::PageHandler> receiver,
      content::WebUI* web_ui,
      EmojiUI* webui_controller,
      bool incognito_mode);
  EmojiPageHandler(const EmojiPageHandler&) = delete;
  EmojiPageHandler& operator=(const EmojiPageHandler&) = delete;
  ~EmojiPageHandler() override;

  // emoji_picker::mojom::PageHandler:
  void ShowUI() override;
  void InsertEmoji(const std::string& emoji_to_insert,
                   bool is_variant) override;
  void IsIncognitoTextField(IsIncognitoTextFieldCallback callback) override;

 private:
  mojo::Receiver<emoji_picker::mojom::PageHandler> receiver_;

  EmojiUI* const webui_controller_;
  bool incognito_mode_;
};
}  // namespace chromeos
#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_EMOJI_EMOJI_PAGE_HANDLER_H_
