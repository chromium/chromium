// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/glic/glic_view.h"

#include "base/command_line.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/glic_button.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/web_contents.h"
#include "ui/events/event_observer.h"
#include "ui/views/event_monitor.h"
#include "ui/views/widget/widget.h"

namespace {
// Default value for how close the corner of glic has to be from a browser's
// glic button to snap.
constexpr static int kSnapDistanceThreshold = 50;

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
        gfx::Point mouse_location = event_monitor_->GetLastMouseLocation();
        views::View::ConvertPointFromScreen(glic_view_, &mouse_location);
        glic_view_->DragFromPoint(mouse_location.OffsetFromOrigin());
      }
    }
  }

  raw_ptr<glic::GlicView> glic_view_;
  std::unique_ptr<views::EventMonitor> event_monitor_;
};

}  // namespace

namespace glic {

GlicView::GlicView(Profile* profile, const gfx::Size& initial_size) {
  profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
      profile, ProfileKeepAliveOrigin::kGlicView);
  keep_alive_ = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::GLIC_VIEW, KeepAliveRestartOption::ENABLED);
  auto web_view = std::make_unique<GlicWebView>(profile);
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

void GlicView::DragFromPoint(gfx::Vector2d mouse_location) {
  // This code isn't set up to handle nested run loops. Nested run loops will
  // lead to crashes.
  if (!in_move_loop_) {
    in_move_loop_ = true;
    gfx::Vector2d drag_offset = mouse_location;
    const views::Widget::MoveLoopSource move_loop_source =
        views::Widget::MoveLoopSource::kMouse;
    GetWidget()->RunMoveLoop(drag_offset, move_loop_source,
                             views::Widget::MoveLoopEscapeBehavior::kDontHide);
    HandleBrowserPinning(
        GetWidget()->GetWindowBoundsInScreen().OffsetFromOrigin() +
        mouse_location);
    in_move_loop_ = false;
  }
}

void GlicView::HandleBrowserPinning(gfx::Vector2d mouse_location) {
  views::Widget* widget = GetWidget();
  // Loops through all browsers in activation order with the latest accessed
  // browser first.
  for (Browser* browser : BrowserList::GetInstance()->OrderedByActivation()) {
    views::Widget* window_widget =
        browser->window()->AsBrowserView()->GetWidget();
    // Skips if:
    // - incognito
    // - not visible
    // - is the same widget as glic
    // - is a different profile (uses browser context to check)
    if (browser->profile()->IsOffTheRecord() ||
        !browser->window()->IsVisible() || window_widget == widget ||
        browser->GetWebView()->GetBrowserContext() !=
            web_view()->GetBrowserContext()) {
      continue;
    }
    auto* tab_strip_region_view =
        browser->window()->AsBrowserView()->tab_strip_region_view();
    if (!tab_strip_region_view || !tab_strip_region_view->glic_button()) {
      continue;
    }
    gfx::Rect glic_button_rect =
        tab_strip_region_view->glic_button()->GetBoundsInScreen();

    float glic_button_mouse_distance =
        (glic_button_rect.CenterPoint() -
         gfx::PointAtOffsetFromOrigin(mouse_location))
            .Length();
    if (glic_button_mouse_distance < kSnapDistanceThreshold) {
      MoveToBrowserPinTarget(browser);
      // Close holder window if existing
      if (holder_widget_) {
        holder_widget_->CloseWithReason(
            views::Widget::ClosedReason::kLostFocus);
        holder_widget_.reset();
      }
      // add observer to new parent
      pinned_target_widget_observer_.SetPinnedTargetWidget(window_widget);
      views::Widget::ReparentNativeView(widget->GetNativeView(),
                                        window_widget->GetNativeView());
    } else if (widget->parent() == window_widget) {
      // If farther than the snapping threshold from the current parent
      // widget, open a blank holder window to reparent to
      MaybeCreateHolderWindowAndReparent(widget);
    }
  }
}

void GlicView::MoveToBrowserPinTarget(Browser* browser) {
  views::Widget* widget = GetWidget();
  gfx::Rect glic_rect = widget->GetWindowBoundsInScreen();
  // TODO fix exact snap location
  gfx::Rect glic_button_rect = browser->window()
                                   ->AsBrowserView()
                                   ->tab_strip_region_view()
                                   ->glic_button()
                                   ->GetBoundsInScreen();
  gfx::Point top_right = glic_button_rect.top_right();
  int tab_strip_padding = GetLayoutConstant(TAB_STRIP_PADDING);
  glic_rect.set_x(top_right.x() - glic_rect.width() - tab_strip_padding);
  glic_rect.set_y(top_right.y() + tab_strip_padding);
  widget->SetBounds(glic_rect);
}

void GlicView::MaybeCreateHolderWindowAndReparent(views::Widget* widget) {
  pinned_target_widget_observer_.SetPinnedTargetWidget(nullptr);
  if (!holder_widget_) {
    holder_widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.activatable = views::Widget::InitParams::Activatable::kNo;
    params.accept_events = false;
    // Name specified for debug purposes
    params.name = "HolderWindow";
    params.bounds = gfx::Rect(0, 0, 0, 0);
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
    holder_widget_->Init(std::move(params));
  }
  views::Widget::ReparentNativeView(widget->GetNativeView(),
                                    holder_widget_->GetNativeView());
}

///////////////////////////////////////////////////////////////////////////////
// PinnedTargetWidgetObserver implementations:
GlicView::PinnedTargetWidgetObserver::PinnedTargetWidgetObserver(GlicView* glic)
    : glic_view_(glic) {}

GlicView::PinnedTargetWidgetObserver::~PinnedTargetWidgetObserver() {
  SetPinnedTargetWidget(nullptr);
}

void GlicView::PinnedTargetWidgetObserver::SetPinnedTargetWidget(
    views::Widget* widget) {
  if (widget == pinned_target_widget_) {
    return;
  }
  if (pinned_target_widget_ && pinned_target_widget_->HasObserver(this)) {
    pinned_target_widget_->RemoveObserver(this);
    pinned_target_widget_ = nullptr;
  }
  if (widget && !widget->HasObserver(this)) {
    widget->AddObserver(this);
    pinned_target_widget_ = widget;
  }
}

void GlicView::PinnedTargetWidgetObserver::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  glic_view_->MoveToBrowserPinTarget(
      chrome::FindBrowserWithWindow(widget->GetNativeWindow()));
}

void GlicView::PinnedTargetWidgetObserver::OnWidgetDestroying(
    views::Widget* widget) {
  SetPinnedTargetWidget(nullptr);
}
}  // namespace glic
