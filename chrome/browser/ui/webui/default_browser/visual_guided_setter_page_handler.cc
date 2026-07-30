// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/default_browser/visual_guided_setter_page_handler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/path_service.h"
#include "chrome/browser/default_browser/default_browser_features.h"
#include "chrome/installer/util/shell_util.h"
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

  gfx::Rect validated_rect = rect;
  if (!rect.IsEmpty()) {
    if (rect.x() < 0 || rect.y() < 0) {
      mojo::ReportBadMessage("Invalid anchor rect bounds.");
      return;
    }

    const gfx::Size container_size = web_contents_->GetContainerBounds().size();
    if (!container_size.IsEmpty() &&
        (rect.right() > container_size.width() ||
         rect.bottom() > container_size.height())) {
      validated_rect = gfx::Rect();
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
    controller_->SetErrorCallback(
        base::BindRepeating(&VisualGuidedSetterPageHandler::OnErrorStateChanged,
                            weak_ptr_factory_.GetWeakPtr()));

    controller_->SetAnchorRectInWebUi(validated_rect);
    controller_->Start();
  } else if (default_browser::IsVisualGuidedSetterDockingEnabled()) {
    controller_->SetAnchorRectInWebUi(validated_rect);
  }
}

void VisualGuidedSetterPageHandler::OpenSettings() {
  base::FilePath chrome_exe;
  if (base::PathService::Get(base::FILE_EXE, &chrome_exe)) {
    ShellUtil::ShowMakeChromeDefaultSystemUI(chrome_exe);
  }
}

void VisualGuidedSetterPageHandler::OnErrorStateChanged(bool has_error) {
  if (page_.is_bound()) {
    page_->SetErrorState(has_error);
  }
}
