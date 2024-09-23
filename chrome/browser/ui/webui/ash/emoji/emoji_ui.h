// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_EMOJI_EMOJI_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_EMOJI_EMOJI_UI_H_

#include <memory>

#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"
#include "chrome/browser/ui/webui/ash/emoji/emoji_page_handler.h"
#include "chrome/browser/ui/webui/ash/emoji/emoji_picker.mojom.h"
#include "chrome/browser/ui/webui/ash/emoji/emoji_search_proxy.h"
#include "chrome/browser/ui/webui/ash/emoji/new_window_proxy.h"
#include "chrome/browser/ui/webui/ash/emoji/new_window_proxy.mojom.h"
#include "chrome/browser/ui/webui/ash/emoji/seal.h"
#include "chrome/browser/ui/webui/ash/emoji/seal.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/browser/ui/webui/webui_load_timer.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/components/emoji/emoji_search.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace ui {
class ColorChangeHandler;
enum class EmojiPickerCategory;
enum class EmojiPickerFocusBehavior;
}  // namespace ui

namespace ash {

class EmojiUI;

// WebUIConfig for chrome://emoji-picker
class EmojiUIConfig : public DefaultTopChromeWebUIConfig<EmojiUI> {
 public:
  EmojiUIConfig();

  // DefaultTopChromeWebUIConfig:
  bool ShouldAutoResizeHost() override;
};

class EmojiUI : public TopChromeWebUIController,
                public emoji_picker::mojom::PageHandlerFactory {
 public:
  explicit EmojiUI(content::WebUI* web_ui);
  EmojiUI(const EmojiUI&) = delete;
  EmojiUI& operator=(const EmojiUI&) = delete;
  ~EmojiUI() override;

  static bool ShouldShow(const ui::TextInputClient* input_client,
                         ui::EmojiPickerFocusBehavior focus_behavior);
  static void Show(ui::EmojiPickerCategory category,
                   ui::EmojiPickerFocusBehavior focus_behavior,
                   const std::string& initial_query);

  // Instantiates the implementor of the mojom::PageHandler mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

  void BindInterface(
      mojo::PendingReceiver<emoji_search::mojom::EmojiSearch> receiver);

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<emoji_picker::mojom::PageHandlerFactory> receiver);

  // Instantiates the implementor of the mojom::NewWindowProxy mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<new_window_proxy::mojom::NewWindowProxy> receiver);

  // Instantiates the implementor of the seal::mojom::SealService mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(mojo::PendingReceiver<seal::mojom::SealService> receiver);

  // emoji_picker::mojom::PageHandlerFactory
  void CreatePageHandler(mojo::PendingReceiver<emoji_picker::mojom::PageHandler>
                             receiver) override;

  static constexpr std::string GetWebUIName() { return "Emoji"; }

 private:
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;
  std::unique_ptr<EmojiPageHandler> page_handler_;
  std::unique_ptr<ash::NewWindowProxy> new_window_proxy_;
  std::unique_ptr<ash::SealService> seal_service_;
  std::unique_ptr<EmojiSearchProxy> emoji_search_;

  mojo::Receiver<emoji_picker::mojom::PageHandlerFactory>
      page_factory_receiver_{this};
  bool incognito_mode_ = false;
  bool no_text_field_ = false;
  emoji_picker::mojom::Category initial_category_;
  std::string initial_query_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_EMOJI_EMOJI_UI_H_
