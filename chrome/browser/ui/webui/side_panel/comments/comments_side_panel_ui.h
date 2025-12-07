// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMMENTS_COMMENTS_SIDE_PANEL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMMENTS_COMMENTS_SIDE_PANEL_UI_H_

#include "chrome/browser/ui/webui/side_panel/comments/comments.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

class CommentsPageHandler;
class CommentsSidePanelUI;

class CommentsSidePanelUIConfig
    : public DefaultTopChromeWebUIConfig<CommentsSidePanelUI> {
 public:
  CommentsSidePanelUIConfig();
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
  std::optional<int> GetCommandIdForTesting() override;
};

class CommentsSidePanelUI : public TopChromeWebUIController,
                            public comments::mojom::PageHandlerFactory {
 public:
  explicit CommentsSidePanelUI(content::WebUI* web_ui);
  CommentsSidePanelUI(const CommentsSidePanelUI&) = delete;
  CommentsSidePanelUI& operator=(const CommentsSidePanelUI&) = delete;
  ~CommentsSidePanelUI() override;

  static constexpr std::string_view GetWebUIName() {
    return "CommentsSidePanel";
  }

  void BindInterface(
      mojo::PendingReceiver<comments::mojom::PageHandlerFactory> receiver);

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();

  // comments::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<comments::mojom::Page> page,
      mojo::PendingReceiver<comments::mojom::PageHandler> receiver) override;

  std::unique_ptr<CommentsPageHandler> comments_page_handler_;
  mojo::Receiver<comments::mojom::PageHandlerFactory>
      comments_page_factory_receiver_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMMENTS_COMMENTS_SIDE_PANEL_UI_H_
