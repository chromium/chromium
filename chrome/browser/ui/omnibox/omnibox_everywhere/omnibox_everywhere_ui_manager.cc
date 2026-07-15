// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_ui_manager.h"

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_widget_delegate.h"
#include "ui/views/widget/widget.h"

namespace omnibox_everywhere {

OmniboxEverywhereUIManager::OmniboxEverywhereUIManager() = default;

OmniboxEverywhereUIManager::~OmniboxEverywhereUIManager() = default;

void OmniboxEverywhereUIManager::Show(gfx::NativeWindow context) {
  if (!widget_) {
    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
    params.shadow_type = views::Widget::InitParams::ShadowType::kDefault;
    widget_delegate_ = std::make_unique<OmniboxEverywhereWidgetDelegate>();
    params.delegate = widget_delegate_.get();
    if (context) {
      params.context = context;
    }

    widget_->Init(std::move(params));
    widget_->MakeCloseSynchronous(base::BindOnce(
        &OmniboxEverywhereUIManager::OnWidgetClosed, base::Unretained(this)));
    widget_observation_.Observe(widget_.get());
  }

  widget_->Show();
  widget_->Activate();
}

void OmniboxEverywhereUIManager::Close() {
  if (widget_) {
    widget_->Close();
  }
}

void OmniboxEverywhereUIManager::CleanUpWidget() {
  if (widget_) {
    widget_observation_.Reset();
    // Release both the widget and the delegate to be destroyed asynchronously
    // in the correct order.
    auto widget = std::move(widget_);
    auto delegate = std::move(widget_delegate_);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](std::unique_ptr<views::Widget> widget,
               std::unique_ptr<OmniboxEverywhereWidgetDelegate> delegate) {
              widget.reset();
              delegate.reset();
            },
            std::move(widget), std::move(delegate)));
  }
}

void OmniboxEverywhereUIManager::OnWidgetDestroying(views::Widget* widget) {
  CleanUpWidget();
}

void OmniboxEverywhereUIManager::OnWidgetClosed(
    views::Widget::ClosedReason reason) {
  CleanUpWidget();
}

}  // namespace omnibox_everywhere
