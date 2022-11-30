// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEED_FEED_UI_H_
#define CHROME_BROWSER_UI_WEBUI_FEED_FEED_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/feed/feed.mojom.h"
#include "chrome/browser/ui/webui/feed/feed_handler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/untrusted_bubble_web_ui_controller.h"

namespace feed {

class FeedUI : public ui::UntrustedBubbleWebUIController,
               public feed::mojom::FeedSidePanelHandlerFactory {
 public:
  explicit FeedUI(content::WebUI* web_ui);

  FeedUI(const FeedUI&) = delete;
  FeedUI& operator=(const FeedUI&) = delete;
  ~FeedUI() override;
  void BindInterface(
      mojo::PendingReceiver<feed::mojom::FeedSidePanelHandlerFactory> factory);

 private:
  void CreateFeedSidePanelHandler(
      mojo::PendingReceiver<feed::mojom::FeedSidePanelHandler> handler,
      mojo::PendingRemote<feed::mojom::FeedSidePanel> side_panel) override;
  mojo::Receiver<feed::mojom::FeedSidePanelHandlerFactory>
      side_panel_handler_factory_{this};
  std::unique_ptr<FeedHandler> side_panel_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace feed

#endif  // CHROME_BROWSER_UI_WEBUI_FEED_FEED_UI_H_
