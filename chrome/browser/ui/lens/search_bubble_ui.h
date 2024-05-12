// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_SEARCH_BUBBLE_UI_H_
#define CHROME_BROWSER_UI_LENS_SEARCH_BUBBLE_UI_H_

#include "chrome/browser/lens/core/mojom/search_bubble.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "content/public/browser/webui_config.h"

namespace lens {

class SearchBubblePageHandler;

class SearchBubbleUI : public TopChromeWebUIController,
                       public lens::mojom::SearchBubblePageHandlerFactory {
 public:
  explicit SearchBubbleUI(content::WebUI* web_ui);
  SearchBubbleUI(const SearchBubbleUI&) = delete;
  SearchBubbleUI& operator=(const SearchBubbleUI&) = delete;
  ~SearchBubbleUI() override;

  void BindInterface(
      mojo::PendingReceiver<lens::mojom::SearchBubblePageHandlerFactory>
          receiver);

  static constexpr std::string GetWebUIName() { return "LensSearchBubble"; }

 private:
  // lens::mojom::SearchBubblePageHandlerFactory
  void CreatePageHandler(
      mojo::PendingRemote<lens::mojom::SearchBubblePage> page,
      mojo::PendingReceiver<lens::mojom::SearchBubblePageHandler> receiver)
      override;

  std::unique_ptr<SearchBubblePageHandler> page_handler_;
  raw_ptr<content::WebUI> web_ui_;

  mojo::Receiver<lens::mojom::SearchBubblePageHandlerFactory>
      page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

// WebUIConfig for chrome://lens-search-bubble.
class SearchBubbleUIConfig
    : public content::DefaultWebUIConfig<SearchBubbleUI> {
 public:
  SearchBubbleUIConfig();

  // content::DefaultWebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_SEARCH_BUBBLE_UI_H_
