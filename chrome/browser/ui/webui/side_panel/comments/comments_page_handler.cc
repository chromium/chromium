// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/comments/comments_page_handler.h"

#include "chrome/browser/ui/webui/side_panel/comments/comments.mojom.h"
#include "chrome/browser/ui/webui/side_panel/comments/comments_side_panel_ui.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"

CommentsPageHandler::CommentsPageHandler(
    mojo::PendingReceiver<comments::mojom::PageHandler> receiver,
    mojo::PendingRemote<comments::mojom::Page> page,
    CommentsSidePanelUI& comments_ui,
    content::WebUI& web_ui)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      web_ui_(web_ui),
      comments_ui_(comments_ui) {}

CommentsPageHandler::~CommentsPageHandler() = default;

void CommentsPageHandler::ShowUI() {
  auto embedder = comments_ui_->embedder();
  if (embedder) {
    embedder->ShowUI();
  }
}
