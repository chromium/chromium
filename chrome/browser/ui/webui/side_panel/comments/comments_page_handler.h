// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMMENTS_COMMENTS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMMENTS_COMMENTS_PAGE_HANDLER_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/webui/side_panel/comments/comments.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class CommentsSidePanelUI;
class BrowserWindowInterface;

namespace content {
class WebUI;
}

// Page handler for the comments side panel WebUI.
class CommentsPageHandler : public comments::mojom::PageHandler {
 public:
  explicit CommentsPageHandler(
      mojo::PendingReceiver<comments::mojom::PageHandler> receiver,
      mojo::PendingRemote<comments::mojom::Page> page,
      CommentsSidePanelUI& comments_ui,
      content::WebUI& web_ui);
  CommentsPageHandler(const CommentsPageHandler&) = delete;
  CommentsPageHandler& operator=(const PageHandler&) = delete;
  ~CommentsPageHandler() override;

  // comments::mojom::PageHandler:
  // Shows the comments side panel UI.
  void ShowUI() override;

 private:
  mojo::Receiver<comments::mojom::PageHandler> receiver_;
  mojo::Remote<comments::mojom::Page> page_;
  const raw_ref<content::WebUI> web_ui_;
  const raw_ref<CommentsSidePanelUI> comments_ui_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMMENTS_COMMENTS_PAGE_HANDLER_H_
