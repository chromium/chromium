// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_EMOJI_EMOJI_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_EMOJI_EMOJI_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/chromeos/emoji/emoji_page_handler.h"
#include "chrome/browser/ui/webui/chromeos/emoji/emoji_picker.mojom.h"
#include "chrome/browser/ui/webui/webui_load_timer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"

#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"
class Profile;

namespace chromeos {

class EmojiUI : public ui::MojoBubbleWebUIController,
                public emoji_picker::mojom::PageHandlerFactory {
 public:
  explicit EmojiUI(content::WebUI* web_ui);
  EmojiUI(const EmojiUI&) = delete;
  EmojiUI& operator=(const EmojiUI&) = delete;
  ~EmojiUI() override;

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
  bool incognito_mode_;
  bool no_text_field_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::EmojiUI;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_EMOJI_EMOJI_UI_H_
