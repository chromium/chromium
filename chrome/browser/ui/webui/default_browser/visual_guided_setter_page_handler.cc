// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/default_browser/visual_guided_setter_page_handler.h"

#include <utility>

#include "content/public/browser/web_contents.h"
#include "ui/views/widget/widget.h"

VisualGuidedSetterPageHandler::VisualGuidedSetterPageHandler(
    mojo::PendingReceiver<visual_guided_setter::mojom::PageHandler> receiver,
    mojo::PendingRemote<visual_guided_setter::mojom::Page> page,
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {}

VisualGuidedSetterPageHandler::~VisualGuidedSetterPageHandler() = default;

void VisualGuidedSetterPageHandler::SetAnchorRect(const gfx::Rect& rect) {
  if (!web_contents_) {
    return;
  }

  if (!rect.IsEmpty()) {
    if (rect.x() < 0 || rect.y() < 0) {
      mojo::ReportBadMessage("Invalid anchor rect bounds.");
      return;
    }

    const gfx::Size container_size = web_contents_->GetContainerBounds().size();
    if (!container_size.IsEmpty() &&
        (rect.right() > container_size.width() ||
         rect.bottom() > container_size.height())) {
      return;
    }
  }

  views::Widget* widget = views::Widget::GetTopLevelWidgetForNativeView(
      web_contents_->GetNativeView());
  if (!widget) {
    return;
  }

  if (!controller_) {
    controller_ = std::make_unique<VisualGuidedSetterControllerWin>(widget);
    controller_->SetWebContents(web_contents_);
  }

  controller_->SetAnchorRectInWebUi(rect);
  if (!controller_->is_running()) {
    controller_->Start();
  }
}
