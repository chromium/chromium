// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_view.h"

#include <string_view>

#include "base/command_line.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/tabs/tab_types.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_view_interface.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/grit/theme_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/hit_test.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/theme_provider.h"
#include "ui/color/color_id.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_observer.h"
#include "ui/views/window/hit_test_utils.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/taskbar/taskbar_decorator_win.h"
#include "ui/display/win/screen_win.h"
#include "ui/views/win/hwnd_util.h"
#endif

namespace {
constexpr std::string_view kShowBrowserFrameRegionsCommandLineSwitch =
    "show-browser-frame-regions";
class ShowBrowserFrameRegionsView : public views::View {
  METADATA_HEADER(ShowBrowserFrameRegionsView, views::View)
 public:
  explicit ShowBrowserFrameRegionsView(BrowserFrameView& frame)
      : browser_widget_(frame) {
    SetCanProcessEventsWithinSubtree(false);
    GetViewAccessibility().SetIsIgnored(true);
    SetProperty(views::kViewIgnoredByLayoutKey, true);
    SetFlipCanvasOnPaintForRTLUI(true);
  }
  ~ShowBrowserFrameRegionsView() override = default;

  static views::View* Find(views::View& parent) {
    for (auto& child : parent.children()) {
      if (views::IsViewClass<ShowBrowserFrameRegionsView>(child)) {
        return child;
      }
    }
    return nullptr;
  }

  void OnPaint(gfx::Canvas* canvas) override {
    const auto params = browser_widget_->GetBrowserLayoutParams();
    const gfx::RectF rect(params.visual_client_area);
    canvas->DrawRect(rect, SK_ColorCYAN);
    if (!params.leading_exclusion.IsEmpty()) {
      canvas->DrawRect(
          gfx::RectF(rect.origin(), params.leading_exclusion.content),
          SK_ColorMAGENTA);
      canvas->DrawRect(
          gfx::RectF(rect.origin(),
                     params.leading_exclusion.ContentWithPadding()),
          SK_ColorRED);
    }
    if (!params.trailing_exclusion.IsEmpty()) {
      const auto& trailing = params.trailing_exclusion;
      canvas->DrawRect(
          gfx::RectF(rect.right() - trailing.content.width(), rect.y(),
                     trailing.content.width(), trailing.content.height()),
          SK_ColorGREEN);
      const auto with_padding = trailing.ContentWithPadding();
      canvas->DrawRect(gfx::RectF(rect.right() - with_padding.width(), rect.y(),
                                  with_padding.width(), with_padding.height()),
                       SK_ColorYELLOW);
    }
  }

 private:
  const raw_ref<BrowserFrameView> browser_widget_;
};

BEGIN_METADATA(ShowBrowserFrameRegionsView)
END_METADATA
}  // namespace

// Tracks the browser view and clears out the pointer when it is destroyed.
// Because of the way widgets are torn down, there will be a brief moment where
// the frame exists but the contents view does not, so maintaining a reference
// from the frame to the contents view that is never cleared is unsafe.
//
// Dereferences of `BrowserFrameView::browser_view()` would have previously
// been UAFs in this situation; now they will be explicit null dereferences
// (which is safer).
//
// See https://crbug.com/465209325 for an example of this happening.
class BrowserFrameView::BrowserViewWatcher : public views::ViewObserver {
 public:
  BrowserViewWatcher(BrowserFrameView& frame, BrowserView* browser_view)
      : frame_(frame) {
    observation_.Observe(browser_view);
  }
  ~BrowserViewWatcher() override = default;

  void OnViewIsDeleting(View* observed_view) override {
    frame_->browser_view_ = nullptr;
    observation_.Reset();
  }

 private:
  const raw_ref<BrowserFrameView> frame_;
  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
};

gfx::Rect BrowserFrameView::BoundsAndMargins::ToEnclosingRect() const {
  gfx::RectF temp = bounds;
  temp.Outset(margins);
  return gfx::ToEnclosingRect(temp);
}

BrowserFrameView::BrowserFrameView(BrowserWidget* browser_widget,
                                   BrowserView* browser_view)
    : browser_widget_(browser_widget),
      browser_view_(browser_view),
      browser_view_watcher_(
          std::make_unique<BrowserViewWatcher>(*this, browser_view)) {
  DCHECK(browser_widget_);
  DCHECK(browser_view_);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kShowBrowserFrameRegionsCommandLineSwitch)) {
    AddChildView(std::make_unique<ShowBrowserFrameRegionsView>(*this));
  }
}

BrowserFrameView::~BrowserFrameView() = default;

BrowserLayoutParams BrowserFrameView::GetBrowserLayoutParams() const {
  BrowserLayoutParams params;
  params.visual_client_area = GetBoundsForClientView();
  const auto caption_bounds = GetCaptionButtonBounds();
  if (caption_bounds.bounds.IsEmpty()) {
    return params;
  }
  const float caption_height =
      caption_bounds.bounds.bottom() - params.visual_client_area.y();
  if (CaptionButtonsOnLeadingEdge()) {
    params.leading_exclusion.content = gfx::SizeF(
        caption_bounds.bounds.right() - params.visual_client_area.x(),
        caption_height);
    params.leading_exclusion.horizontal_padding =
        caption_bounds.margins.right();
    params.leading_exclusion.vertical_padding = caption_bounds.margins.bottom();
  } else {
    params.trailing_exclusion.content = gfx::SizeF(
        params.visual_client_area.right() - caption_bounds.bounds.x(),
        caption_height);
    params.trailing_exclusion.horizontal_padding =
        caption_bounds.margins.left();
    params.trailing_exclusion.vertical_padding =
        caption_bounds.margins.bottom();
  }
  return params;
}

void BrowserFrameView::OnBrowserViewInitViewsComplete() {
  UpdateMinimumSize();
}

void BrowserFrameView::OnFullscreenStateChanged() {}

bool BrowserFrameView::CaptionButtonsOnLeadingEdge() const {
  return false;
}

bool BrowserFrameView::CaptionButtonsOnTrailingEdge() const {
  return !CaptionButtonsOnLeadingEdge();
}

void BrowserFrameView::LayoutWebAppWindowTitle(
    const gfx::Rect& available_space,
    views::Label& window_title_label) const {
  // Default is no title.
  window_title_label.SetVisible(false);
}

void BrowserFrameView::UpdateFullscreenTopUI() {}

bool BrowserFrameView::ShouldHideTopUIInFullscreen() const {
  return true;
}

bool BrowserFrameView::ShouldShowWebAppFrameToolbar() const {
  if (browser_widget_->IsFullscreen() && ShouldHideTopUIInFullscreen()) {
    return false;
  }
  return true;
}

bool BrowserFrameView::IsFrameCondensed() const {
  return browser_widget_->IsMaximized() || browser_widget_->IsFullscreen();
}

bool BrowserFrameView::HasVisibleBackgroundTabShapes(
    BrowserFrameActiveState active_state) const {
  DCHECK(browser_view_->GetSupportsTabStrip());

  const bool active = ShouldPaintAsActiveForState(active_state);
  const std::optional<int> bg_id = GetCustomBackgroundId(active_state);
  if (bg_id.has_value()) {
    // If the theme has a custom tab background image, assume tab shapes are
    // visible.  This is pessimistic; the theme may use the same image as the
    // frame, just shifted to align, or a solid-color image the same color as
    // the frame; but to detect this we'd need to do some kind of aligned
    // rendering comparison, which seems not worth it.
    const ui::ThemeProvider* tp = GetThemeProvider();
    if (tp->HasCustomImage(bg_id.value())) {
      return true;
    }

    // Inactive tab background images are copied from the active ones, so in the
    // inactive case, check the active image as well.
    if (!active) {
      const int active_id = browser_view_->GetIncognito()
                                ? IDR_THEME_TAB_BACKGROUND_INCOGNITO
                                : IDR_THEME_TAB_BACKGROUND;
      if (tp->HasCustomImage(active_id)) {
        return true;
      }
    }

    // The tab image is a tinted version of the frame image.  Tabs are visible
    // iff the tint has some visible effect.
    return color_utils::IsHSLShiftMeaningful(
        tp->GetTint(ThemeProperties::TINT_BACKGROUND_TAB));
  }

  // Background tab shapes are visible iff the tab color differs from the frame
  // color.
  return TabStyle::Get()->GetTabBackgroundColor(
             TabStyle::TabSelectionState::kInactive,
             /*hovered=*/false, ShouldPaintAsActiveForState(active_state),
             *GetColorProvider()) != GetFrameColor(active_state);
}

SkColor BrowserFrameView::GetCaptionColor(
    BrowserFrameActiveState active_state) const {
  return GetColorProvider()->GetColor(ShouldPaintAsActiveForState(active_state)
                                          ? kColorFrameCaptionActive
                                          : kColorFrameCaptionInactive);
}

SkColor BrowserFrameView::GetFrameColor(
    BrowserFrameActiveState active_state) const {
  return GetColorProvider()->GetColor(ShouldPaintAsActiveForState(active_state)
                                          ? ui::kColorFrameActive
                                          : ui::kColorFrameInactive);
}

std::optional<int> BrowserFrameView::GetCustomBackgroundId(
    BrowserFrameActiveState active_state) const {
  const ui::ThemeProvider* tp = GetThemeProvider();
  const bool incognito = browser_view_->GetIncognito();
  const bool active = ShouldPaintAsActiveForState(active_state);
  const int active_id =
      incognito ? IDR_THEME_TAB_BACKGROUND_INCOGNITO : IDR_THEME_TAB_BACKGROUND;
  const int inactive_id = incognito
                              ? IDR_THEME_TAB_BACKGROUND_INCOGNITO_INACTIVE
                              : IDR_THEME_TAB_BACKGROUND_INACTIVE;
  const int id = active ? active_id : inactive_id;

  // tp->HasCustomImage() will only return true if the supplied ID has been
  // customized directly.  We also account for the following fallback cases:
  // * The inactive images are copied directly from the active ones if present
  // * Tab backgrounds are generated from frame backgrounds if present, and
  // * The incognito frame image is generated from the normal frame image, so
  //   in incognito mode we look at both.
  const bool has_custom_image =
      tp->HasCustomImage(id) || (!active && tp->HasCustomImage(active_id)) ||
      tp->HasCustomImage(IDR_THEME_FRAME) ||
      (incognito && tp->HasCustomImage(IDR_THEME_FRAME_INCOGNITO));
  return has_custom_image ? std::make_optional(id) : std::nullopt;
}

void BrowserFrameView::UpdateMinimumSize() {}

gfx::Insets BrowserFrameView::RestoredMirroredFrameBorderInsets() const {
  NOTREACHED();
}

gfx::Insets BrowserFrameView::GetInputInsets() const {
  NOTREACHED();
}

SkRRect BrowserFrameView::GetRestoredClipRegion() const {
  NOTREACHED();
}

int BrowserFrameView::GetTranslucentTopAreaHeight() const {
  return 0;
}

void BrowserFrameView::SetFrameBounds(const gfx::Rect& bounds) {
  browser_widget_->SetBounds(bounds);
}

void BrowserFrameView::PaintAsActiveChanged() {
  // Changing the activation state may change the visible frame color.
  SchedulePaint();
}

BrowserFrameView::BoundsAndMargins BrowserFrameView::GetCaptionButtonBounds()
    const {
  // This is a hacky solution that uses existing logic to compute bounds.
  // It should ideally be overridden with platform-appropriate code.
  const int fallback_height = TabStyle::Get()->GetStandardHeight();
  const gfx::Rect proposed_tabstrip_bounds =
      GetBoundsForTabStripRegion(gfx::Size(0, fallback_height));
  gfx::RectF bounds;
  if (CaptionButtonsOnLeadingEdge()) {
    bounds = gfx::RectF(0, 0, proposed_tabstrip_bounds.x(),
                        proposed_tabstrip_bounds.bottom());
  } else {
    const float x = proposed_tabstrip_bounds.right();
    bounds = gfx::RectF(x, 0, width() - x, proposed_tabstrip_bounds.bottom());
  }
  // Because we only have the tabstrip region bounds to work from, it is not
  // possible to determine which part of the region is button and which is
  // padding; therefore assume all of it is button.
  return BoundsAndMargins{bounds};
}

bool BrowserFrameView::ShouldPaintAsActiveForState(
    BrowserFrameActiveState active_state) const {
  return (active_state == BrowserFrameActiveState::kUseCurrent)
             ? FrameView::ShouldPaintAsActive()
             : (active_state == BrowserFrameActiveState::kActive);
}

gfx::ImageSkia BrowserFrameView::GetFrameImage(
    BrowserFrameActiveState active_state) const {
  const ui::ThemeProvider* tp = GetThemeProvider();
  const int frame_image_id = ShouldPaintAsActiveForState(active_state)
                                 ? IDR_THEME_FRAME
                                 : IDR_THEME_FRAME_INACTIVE;
  return (tp->HasCustomImage(frame_image_id) ||
          tp->HasCustomImage(IDR_THEME_FRAME))
             ? *tp->GetImageSkiaNamed(frame_image_id)
             : gfx::ImageSkia();
}

gfx::ImageSkia BrowserFrameView::GetFrameOverlayImage(
    BrowserFrameActiveState active_state) const {
  if (browser_view_->GetIncognito() || !browser_view_->GetIsNormalType()) {
    return gfx::ImageSkia();
  }

  const ui::ThemeProvider* tp = GetThemeProvider();
  const int frame_overlay_image_id = ShouldPaintAsActiveForState(active_state)
                                         ? IDR_THEME_FRAME_OVERLAY
                                         : IDR_THEME_FRAME_OVERLAY_INACTIVE;
  return tp->HasCustomImage(frame_overlay_image_id)
             ? *tp->GetImageSkiaNamed(frame_overlay_image_id)
             : gfx::ImageSkia();
}

void BrowserFrameView::Layout(PassKey p) {
  LayoutSuperclass<FrameView>(this);
  // If the show browser frame view is present, make it fills this entire frame.
  if (auto* view = ShowBrowserFrameRegionsView::Find(*this)) {
    view->SetBoundsRect(GetLocalBounds());
  }
}

views::View::Views BrowserFrameView::GetChildrenInZOrder() {
  auto views = FrameView::GetChildrenInZOrder();
  // If the show browser frame view is in the list, move it to the end so it
  // paints over everything.
  if (auto* view = ShowBrowserFrameRegionsView::Find(*this)) {
    if (std::erase(views, view)) {
      views.push_back(view);
    }
  }
  return views;
}

#if BUILDFLAG(IS_WIN)
// Sending the WM_NCPOINTERDOWN, WM_NCPOINTERUPDATE, and WM_NCPOINTERUP to the
// default window proc does not bring up the system menu on long press, so we
// use the gesture recognizer to turn it into a LONG_TAP gesture and handle it
// here. See https://crbug.com/1327506 for more info.
void BrowserFrameView::OnGestureEvent(ui::GestureEvent* event) {
  gfx::Point event_loc = event->location();
  // This opens the title bar system context menu on long press in the titlebar.
  // NonClientHitTest returns HTCAPTION if `event_loc` is in the empty space on
  // the titlebar.
  if (event->type() == ui::EventType::kGestureLongTap &&
      NonClientHitTest(event_loc) == HTCAPTION) {
    views::View::ConvertPointToScreen(this, &event_loc);
    event_loc = display::win::GetScreenWin()->DIPToScreenPoint(event_loc);
    views::ShowSystemMenuAtScreenPixelLocation(views::HWNDForView(this),
                                               event_loc);
    event->SetHandled();
  }
}

int BrowserFrameView::GetSystemMenuY() const {
  if (!browser_view()->GetTabStripVisible()) {
    return GetTopInset(false);
  }

  // TODO(crbug.com/437915662): Find an alternative way to get the starting Y
  // position when in vertical tabs mode since the top element will now be the
  // toolbar instead of the tabstrip.
  return GetBoundsForTabStripRegion(
             browser_view()->tab_strip_view()->GetMinimumSize())
             .bottom() -
         GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP);
}
#endif  // BUILDFLAG(IS_WIN)

BEGIN_METADATA(BrowserFrameView)
END_METADATA
