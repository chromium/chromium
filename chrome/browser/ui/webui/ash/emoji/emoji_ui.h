// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_EMOJI_EMOJI_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_EMOJI_EMOJI_UI_H_

#include <memory>

#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"
#include "chrome/browser/ui/webui/ash/emoji/emoji_page_handler.h"
#include "chrome/browser/ui/webui/ash/emoji/emoji_picker.mojom.h"
#include "chrome/browser/ui/webui/webui_load_timer.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"

class Profile;

namespace ash {

class EmojiUI;

// WebUIConfig for chrome://emoji-picker
class EmojiUIConfig : public content::DefaultWebUIConfig<EmojiUI> {
 public:
  EmojiUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIEmojiPickerHost) {}
};

class EmojiUI : public ui::MojoBubbleWebUIController,
                public emoji_picker::mojom::PageHandlerFactory {
 public:
  explicit EmojiUI(content::WebUI* web_ui);
  EmojiUI(const EmojiUI&) = delete;
  EmojiUI& operator=(const EmojiUI&) = delete;
  ~EmojiUI() override;

  static bool ShouldShow(const ui::TextInputClient* input_client);
  static void Show(Profile* profile);

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<emoji_picker::mojom::PageHandlerFactory> receiver);

  // emoji_picker::mojom::PageHandlerFactory
  void CreatePageHandler(mojo::PendingReceiver<emoji_picker::mojom::PageHandler>
                             receiver) override;

 private:
  std::unique_ptr<EmojiPageHandler> page_handler_;

  mojo::Receiver<emoji_picker::mojom::PageHandlerFactory>
      page_factory_receiver_{this};
  bool incognito_mode_ = false;
  bool no_text_field_ = false;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_EMOJI_EMOJI_UI_H_
