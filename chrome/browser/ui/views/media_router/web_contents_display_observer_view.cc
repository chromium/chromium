// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/web_contents_display_observer_view.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/browser_list.h"
#include "content/public/browser/web_contents.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

namespace media_router {

// static
std::unique_ptr<WebContentsDisplayObserver> WebContentsDisplayObserver::Create(
    content::WebContents* web_contents,
    base::RepeatingClosure callback) {
  return std::make_unique<WebContentsDisplayObserverView>(web_contents,
                                                          std::move(callback));
}

WebContentsDisplayObserverView::WebContentsDisplayObserverView(
    content::WebContents* web_contents,
    base::RepeatingClosure callback)
    : WebContentsObserver(web_contents),
      web_contents_(web_contents),
      widget_(views::Widget::GetWidgetForNativeWindow(
          web_contents->GetTopLevelNativeWindow())),
      callback_(std::move(callback)) {
  // |widget_| may be null in tests.
  if (widget_) {
    display_ = GetDisplayNearestWidget();
    widget_->AddObserver(this);
  }
  BrowserList::AddObserver(this);
}

WebContentsDisplayObserverView::~WebContentsDisplayObserverView() {
  if (widget_)
    widget_->RemoveObserver(this);
  BrowserList::RemoveObserver(this);
  CHECK(!WidgetObserver::IsInObserverList());
}

void WebContentsDisplayObserverView::OnBrowserSetLastActive(Browser* browser) {
  // This gets called when a browser tab detaches from a window or gets merged
  // into another window. We update the widget to observe, if necessary.
  // If |web_contents_| or |widget_| is null, then we no longer have WebContents
  // to observe.
  if (!web_contents_ || !widget_)
    return;

  views::Widget* new_widget = views::Widget::GetWidgetForNativeWindow(
      web_contents_->GetTopLevelNativeWindow());
  if (new_widget != widget_) {
    widget_->RemoveObserver(this);
    widget_ = new_widget;
    if (widget_) {
      widget_->AddObserver(this);
      CheckForDisplayChange();
    }
  }
}

void WebContentsDisplayObserverView::OnWidgetDestroying(views::Widget* widget) {
  if (widget_)
    widget_->RemoveObserver(this);
  widget_ = nullptr;
}

void WebContentsDisplayObserverView::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  CheckForDisplayChange();
}

const display::Display& WebContentsDisplayObserverView::GetCurrentDisplay()
    const {
  return display_;
}

void WebContentsDisplayObserverView::WebContentsDestroyed() {
  web_contents_ = nullptr;
}

void WebContentsDisplayObserverView::CheckForDisplayChange() {
  display::Display new_display = GetDisplayNearestWidget();
  if (new_display.id() == display_.id())
    return;

  display_ = new_display;
  callback_.Run();
}

display::Display WebContentsDisplayObserverView::GetDisplayNearestWidget()
    const {
  return display::Screen::GetScreen()->GetDisplayNearestWindow(
      widget_->GetNativeWindow());
}

}  // namespace media_router
