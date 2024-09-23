// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "components/omnibox/common/omnibox_features.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/layout/layout_provider.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "ui/views/widget/native_widget_aura.h"
#endif

namespace {

// Value from the spec controlling appearance of the shadow.
constexpr int kElevation = 16;

#if !defined(USE_AURA)

struct WidgetEventPair {
  raw_ptr<views::Widget> widget;
  std::unique_ptr<ui::MouseEvent> event;
};

#if BUILDFLAG(IS_MAC)
views::Widget* GetImmersiveFullscreenWidgetForEvent(
    views::View* this_view,
    const ui::MouseEvent* this_event) {
  views::Widget* parent_widget = this_view->GetWidget()->parent();
  BrowserView* browser_view = BrowserView::GetBrowserViewForNativeWindow(
      parent_widget->GetNativeWindow());

  // If the results window is not a child of the overlay widget we are not in
  // immersive fullscreen.
  if (browser_view->overlay_widget() != parent_widget) {
    return nullptr;
  }

  {
    // If the event is located in the location bar send the event to the overlay
    // widget to handle text selection.
    gfx::Point event_location = this_event->location();
    views::View::ConvertPointToScreen(this_view, &event_location);
    views::View::ConvertPointFromScreen(browser_view->GetLocationBarView(),
                                        &event_location);
    if (browser_view->GetLocationBarView()->HitTestPoint(event_location)) {
      return browser_view->overlay_widget();
    }
  }

  {
    // If the event is located in the content view send the event to the browser
    // widget.
    gfx::Point event_location = this_event->location();
    views::View::ConvertPointToScreen(this_view, &event_location);
    views::View::ConvertPointFromScreen(browser_view->contents_container(),
                                        &event_location);
    if (browser_view->contents_container()->HitTestPoint(event_location)) {
      return browser_view->GetWidget();
    }
  }

  // In immersive fullscreen with tabs enabled the floating results shadow
  // spreads into the tab strip area which is hosted in yet another separate
  // widget, the tab widget. Send the rest of the events to the tab widget. This
  // will allow for tab strip interaction in the area covered by the shadow and
  // accurate tab hover card dismissal.
  if (browser_view->tab_overlay_widget()) {
    return browser_view->tab_overlay_widget();
  }

  // If immersive fullscreen with tabs is not enabled, send events to the
  // overlay widget for tab strip interaction in the area covered by the shadow.
  return browser_view->overlay_widget();
}
#endif

WidgetEventPair GetParentWidgetAndEvent(views::View* this_view,
                                        const ui::MouseEvent* this_event) {
  // Note that the floating results view is a top-level widget, so hop up a
  // level before looking for the parent's top-level widget for event
  // forwarding.
  views::Widget* this_widget = this_view->GetWidget();
  views::Widget* parent_widget = this_widget->parent();
  std::unique_ptr<ui::MouseEvent> event(
      static_cast<ui::MouseEvent*>(this_event->Clone().release()));
  if (!parent_widget)
    return {nullptr, std::move(event)};

// On macOS if the parent widget is the overlay widget we are in immersive
// fullscreen. Don't walk any higher up the tree. The overlay or tab widget will
// handle the event.
// TODO(http://crbug.com/1462791): Remove custom event handling.
#if BUILDFLAG(IS_MAC)
  views::Widget* top_level =
      GetImmersiveFullscreenWidgetForEvent(this_view, this_event)
          ?: parent_widget->GetTopLevelWidgetForNativeView(
                 parent_widget->GetNativeView());
#else
  views::Widget* top_level = parent_widget->GetTopLevelWidgetForNativeView(
      parent_widget->GetNativeView());
#endif

  DCHECK_NE(this_widget, top_level);
  if (!top_level)
    return {nullptr, std::move(event)};

  gfx::Point event_location = this_event->location();
  views::View::ConvertPointToScreen(this_view, &event_location);
  views::View::ConvertPointFromScreen(top_level->GetRootView(),
                                      &event_location);

  // Convert location to top level widget coordinate.
  event->set_location(event_location);

  return {top_level, std::move(event)};
}

#endif  // !USE_AURA

// View at the top of the frame which paints transparent pixels to make a hole
// so that the location bar shows through.
class TopBackgroundView : public views::View {
  METADATA_HEADER(TopBackgroundView, views::View)

 public:
  explicit TopBackgroundView(const LocationBarView* location_bar)
      : location_bar_(location_bar) {}

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    const SkColor background_color =
        GetColorProvider()->GetColor(kColorOmniboxResultsBackground);

    // Paint a stroke of the background color as a 1 px border to hide the
    // underlying antialiased location bar/toolbar edge.  The round rect here is
    // not antialiased, since the goal is to completely cover the underlying
    // pixels, and AA would let those on the edge partly bleed through.
    SetBackground(location_bar_->CreateRoundRectBackground(
        SK_ColorTRANSPARENT, background_color, SkBlendMode::kSrc, false));
  }

#if !defined(USE_AURA)
  // For non-Aura platforms, forward mouse events and cursor requests intended
  // for the omnibox to the proper Widgets/Views. For Aura platforms, this is
  // done with an event targeter set up in
  // RoundedOmniboxResultsFrame::AddedToWidget(), below.
 private:
  // Note that mouse moved events can be dispatched through OnMouseEvent, but
  // RootView directly calls OnMouseMoved as well, so override OnMouseMoved as
  // well to catch 'em all.
  void OnMouseMoved(const ui::MouseEvent& event) override {
    auto pair = GetParentWidgetAndEvent(this, &event);
    if (pair.widget)
      pair.widget->OnMouseEvent(pair.event.get());
  }

  void OnMouseEvent(ui::MouseEvent* event) override {
    auto pair = GetParentWidgetAndEvent(this, event);
    if (pair.widget)
      pair.widget->OnMouseEvent(pair.event.get());

    // If the original event isn't marked as "handled" then it will propagate up
    // the view hierarchy and might be double-handled. https://crbug.com/870341
    event->SetHandled();
  }

  ui::Cursor GetCursor(const ui::MouseEvent& event) override {
    auto pair = GetParentWidgetAndEvent(this, &event);
    if (pair.widget) {
      views::View* omnibox_view =
          pair.widget->GetRootView()->GetEventHandlerForPoint(
              pair.event->location());
      return omnibox_view->GetCursor(*pair.event);
    }

    return ui::Cursor();
  }
#endif  // !USE_AURA

 private:
  raw_ptr<const LocationBarView> location_bar_;
};

BEGIN_METADATA(TopBackgroundView)
END_METADATA

// Insets used to position |contents_| within |contents_host_|.
gfx::Insets GetContentInsets() {
  return gfx::Insets::TLBR(
      RoundedOmniboxResultsFrame::GetNonResultSectionHeight(), 0, 0, 0);
}

}  // namespace

RoundedOmniboxResultsFrame::RoundedOmniboxResultsFrame(
    views::View* contents,
    LocationBarView* location_bar)
    : contents_(contents) {
  // Host the contents in its own View to simplify layout and customization.
  contents_host_ = new views::View();
  contents_host_->SetBackground(
      views::CreateThemedSolidBackground(kColorOmniboxResultsBackground));
  contents_host_->SetPaintToLayer();
  contents_host_->layer()->SetFillsBoundsOpaquely(false);

  // Use rounded corners.
  int corner_radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::ShapeContextTokens::kOmniboxExpandedRadius);
  contents_host_->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(corner_radius));
  contents_host_->layer()->SetIsFastRoundedCorner(true);

  top_background_ = new TopBackgroundView(location_bar);
  contents_host_->AddChildView(top_background_.get());
  contents_host_->AddChildView(contents_.get());

  // Initialize the shadow.
  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::Arrow::NONE,
      views::BubbleBorder::Shadow::STANDARD_SHADOW);
  border->SetCornerRadius(corner_radius);
  border->set_md_shadow_elevation(kElevation);
  SetBorder(std::move(border));

  AddChildView(contents_host_.get());
}

RoundedOmniboxResultsFrame::~RoundedOmniboxResultsFrame() = default;

// static
void RoundedOmniboxResultsFrame::OnBeforeWidgetInit(
    views::Widget::InitParams* params,
    views::Widget* widget) {
#if BUILDFLAG(IS_WIN)
  // On Windows, use an Aura window instead of a native window, because the
  // native window does not support clicking through translucent shadows to the
  // underyling content. Linux and ChromeOS do not need this because they
  // already use Aura for the suggestions dropdown.
  //
  // TODO(sdy): Mac does not support Aura at the moment, and needs a different
  // platform-specific solution.
  params->native_widget = new views::NativeWidgetAura(widget);
#endif
  params->name = "RoundedOmniboxResultsFrameWindow";

  // Since we are drawing the shadow in Views via the BubbleBorder, we never
  // want our widget to have its own window-manager drawn shadow.
  params->shadow_type = views::Widget::InitParams::ShadowType::kNone;
}

// static
int RoundedOmniboxResultsFrame::GetNonResultSectionHeight() {
  return GetLayoutConstant(LOCATION_BAR_HEIGHT) +
         GetLocationBarAlignmentInsets().height();
}

// static
gfx::Insets RoundedOmniboxResultsFrame::GetLocationBarAlignmentInsets() {
  if (ui::TouchUiController::Get()->touch_ui()) {
    return gfx::Insets::TLBR(6, 1, 5, 1);
  }
  return gfx::Insets::VH(5, 6);
}

// static
gfx::Insets RoundedOmniboxResultsFrame::GetShadowInsets() {
  return views::BubbleBorder::GetBorderAndShadowInsets(kElevation);
}

void RoundedOmniboxResultsFrame::Layout(PassKey) {
  // This is called when the Widget resizes due to results changing. Resizing
  // the Widget is fast on ChromeOS, but slow on other platforms, and can't be
  // animated smoothly.
  // TODO(tapted): Investigate using a static Widget size.
  const gfx::Rect bounds = GetContentsBounds();
  contents_host_->SetBoundsRect(bounds);

  gfx::Rect top_bounds(contents_host_->GetContentsBounds());
  top_bounds.set_height(GetNonResultSectionHeight());
  top_bounds.Inset(GetLocationBarAlignmentInsets());
  top_background_->SetBoundsRect(top_bounds);

  gfx::Rect results_bounds(contents_host_->GetContentsBounds());
  results_bounds.Inset(GetContentInsets());
  contents_->SetBoundsRect(results_bounds);
}

void RoundedOmniboxResultsFrame::AddedToWidget() {
#if defined(USE_AURA)
  // Use a ui::EventTargeter that allows mouse and touch events in the top
  // portion of the Widget to pass through to the omnibox beneath it.
  auto results_targeter = std::make_unique<aura::WindowTargeter>();
  results_targeter->SetInsets(GetInsets() + GetContentInsets());
  GetWidget()->GetNativeWindow()->SetEventTargeter(std::move(results_targeter));
#endif  // USE_AURA
}

// Note: The OnMouseMoved function is only called for the shadow area, as mouse-
// moved events are not dispatched through the view hierarchy but are direct-
// dispatched by RootView. This OnMouseEvent function is on the dispatch path
// for all mouse events of the window, so be careful to correctly mark events as
// "handled" above in subviews.
#if !defined(USE_AURA)

// Note that mouse moved events can be dispatched through OnMouseEvent, but
// RootView directly calls OnMouseMoved as well, so override OnMouseMoved as
// well to catch 'em all.
void RoundedOmniboxResultsFrame::OnMouseMoved(const ui::MouseEvent& event) {
  auto pair = GetParentWidgetAndEvent(this, &event);
  if (pair.widget)
    pair.widget->OnMouseEvent(pair.event.get());
}

void RoundedOmniboxResultsFrame::OnMouseEvent(ui::MouseEvent* event) {
  auto pair = GetParentWidgetAndEvent(this, event);
  if (pair.widget)
    pair.widget->OnMouseEvent(pair.event.get());
}

#endif  // !USE_AURA

BEGIN_METADATA(RoundedOmniboxResultsFrame)
END_METADATA
