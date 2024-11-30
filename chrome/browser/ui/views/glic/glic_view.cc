// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/glic/glic_view.h"

#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/web_contents.h"
#include "ui/events/event_observer.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/event_monitor.h"
#include "ui/views/widget/widget.h"

namespace {

// Helper class for observing mouse and key events from native window.
class WindowEventObserver : public ui::EventObserver {
 public:
  explicit WindowEventObserver(glic::GlicView* glic_view)
      : glic_view_(glic_view) {
    event_monitor_ = views::EventMonitor::CreateWindowMonitor(
        this, glic_view_->GetWidget()->GetNativeWindow(),
        {ui::EventType::kMouseDragged});
  }

  WindowEventObserver(const WindowEventObserver&) = delete;
  WindowEventObserver& operator=(const WindowEventObserver&) = delete;
  ~WindowEventObserver() override = default;

  void OnEvent(const ui::Event& event) override {
    if (event.IsMouseEvent()) {
      if (event.type() == ui::EventType::kMouseDragged) {
        gfx::Point mousePoint = event_monitor_->GetLastMouseLocation();
        views::View::ConvertPointFromScreen(glic_view_, &mousePoint);
        glic_view_->DragFromPoint(mousePoint.OffsetFromOrigin());
      }
    }
  }

  raw_ptr<glic::GlicView> glic_view_;
  std::unique_ptr<views::EventMonitor> event_monitor_;
};

}  // namespace

namespace glic {

GlicView::GlicView(Profile* profile, const gfx::Size& initial_size) {
  auto web_view = std::make_unique<views::WebView>(profile);
  web_view_ = web_view.get();
  web_view->SetSize(initial_size);
  web_view->LoadInitialURL(GURL("chrome://glic"));
  web_view->GetWebContents()->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);
  AddChildView(std::move(web_view));
}

GlicView::~GlicView() = default;

// static
std::pair<views::UniqueWidgetPtr, GlicView*> GlicView::CreateWidget(
    Profile* profile,
    const gfx::Rect& initial_bounds) {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.remove_standard_frame = true;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.bounds = initial_bounds;

  views::UniqueWidgetPtr widget =
      std::make_unique<views::Widget>(std::move(params));

  auto glic_view = std::make_unique<GlicView>(profile, initial_bounds.size());
  GlicView* raw_glic_view = glic_view.get();
  widget->SetContentsView(std::move(glic_view));

  return {std::move(widget), raw_glic_view};
}

void GlicView::AddedToWidget() {
  window_event_observer_ = std::make_unique<WindowEventObserver>(this);
}

void GlicView::DragFromPoint(gfx::Vector2d mousePoint) {
  // This code isn't set up to handle nested run loops. Nested run loops will
  // lead to crashes.
  if (!in_move_loop_) {
    in_move_loop_ = true;
    gfx::Vector2d drag_offset = mousePoint;
    const views::Widget::MoveLoopSource move_loop_source =
        views::Widget::MoveLoopSource::kMouse;
    GetWidget()->RunMoveLoop(drag_offset, move_loop_source,
                             views::Widget::MoveLoopEscapeBehavior::kDontHide);
    in_move_loop_ = false;
  }
}
}  // namespace glic
