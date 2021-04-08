// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"

#include "base/metrics/histogram_macros.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_types.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/grit/theme_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/hit_test.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/background.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/window/hit_test_utils.h"

#if defined(OS_WIN)
#include "chrome/browser/taskbar/taskbar_decorator_win.h"
#endif

// static
constexpr int BrowserNonClientFrameView::kMinimumDragHeight;


BrowserNonClientFrameView::BrowserNonClientFrameView(BrowserFrame* frame,
                                                     BrowserView* browser_view)
    : frame_(frame), browser_view_(browser_view) {
  DCHECK(frame_);
  DCHECK(browser_view_);

  // The profile manager may by null in tests.
  if (g_browser_process->profile_manager()) {
    g_browser_process->profile_manager()->
        GetProfileAttributesStorage().AddObserver(this);
  }
  if (browser_view_->tabstrip()) {
    DCHECK(
        !tab_strip_observation_.IsObservingSource(browser_view_->tabstrip()));
    tab_strip_observation_.Observe(browser_view_->tabstrip());
  }
}

BrowserNonClientFrameView::~BrowserNonClientFrameView() {
  // The profile manager may by null in tests.
  if (g_browser_process->profile_manager()) {
    g_browser_process->profile_manager()->
        GetProfileAttributesStorage().RemoveObserver(this);
  }
}

void BrowserNonClientFrameView::OnBrowserViewInitViewsComplete() {
  UpdateMinimumSize();
}

void BrowserNonClientFrameView::OnFullscreenStateChanged() {
  if (frame_->IsFullscreen())
    browser_view_->HideDownloadShelf();
  else
    browser_view_->UnhideDownloadShelf();
}

bool BrowserNonClientFrameView::CaptionButtonsOnLeadingEdge() const {
  return false;
}

void BrowserNonClientFrameView::UpdateFullscreenTopUI() {}

bool BrowserNonClientFrameView::ShouldHideTopUIForFullscreen() const {
  return frame_->IsFullscreen();
}

bool BrowserNonClientFrameView::CanUserExitFullscreen() const {
  return true;
}

bool BrowserNonClientFrameView::IsFrameCondensed() const {
  return frame_->IsMaximized() || frame_->IsFullscreen();
}

bool BrowserNonClientFrameView::HasVisibleBackgroundTabShapes(
    BrowserFrameActiveState active_state) const {
  DCHECK(browser_view_->GetSupportsTabStrip());

  TabStrip* const tab_strip = browser_view_->tabstrip();

  const bool active = ShouldPaintAsActive(active_state);
  const base::Optional<int> bg_id =
      tab_strip->GetCustomBackgroundId(active_state);
  if (bg_id.has_value()) {
    // If the theme has a custom tab background image, assume tab shapes are
    // visible.  This is pessimistic; the theme may use the same image as the
    // frame, just shifted to align, or a solid-color image the same color as
    // the frame; but to detect this we'd need to do some kind of aligned
    // rendering comparison, which seems not worth it.
    const ui::ThemeProvider* tp = GetThemeProvider();
    if (tp->HasCustomImage(bg_id.value()))
      return true;

    // Inactive tab background images are copied from the active ones, so in the
    // inactive case, check the active image as well.
    if (!active) {
      const int active_id = browser_view_->GetIncognito()
                                ? IDR_THEME_TAB_BACKGROUND_INCOGNITO
                                : IDR_THEME_TAB_BACKGROUND;
      if (tp->HasCustomImage(active_id))
        return true;
    }

    // The tab image is a tinted version of the frame image.  Tabs are visible
    // iff the tint has some visible effect.
    return color_utils::IsHSLShiftMeaningful(
        tp->GetTint(ThemeProperties::TINT_BACKGROUND_TAB));
  }

  // Background tab shapes are visible iff the tab color differs from the frame
  // color.
  return tab_strip->GetTabBackgroundColor(TabActive::kInactive, active_state) !=
         GetFrameColor(active_state);
}

bool BrowserNonClientFrameView::EverHasVisibleBackgroundTabShapes() const {
  return HasVisibleBackgroundTabShapes(BrowserFrameActiveState::kActive) ||
         HasVisibleBackgroundTabShapes(BrowserFrameActiveState::kInactive);
}

bool BrowserNonClientFrameView::CanDrawStrokes() const {
  // Web apps should not draw strokes if they don't have a tab strip.
  return !browser_view_->browser()->app_controller() ||
         browser_view_->browser()->app_controller()->has_tab_strip();
}

SkColor BrowserNonClientFrameView::GetCaptionColor(
    BrowserFrameActiveState active_state) const {
  return color_utils::GetColorWithMaxContrast(GetFrameColor(active_state));
}

SkColor BrowserNonClientFrameView::GetFrameColor(
    BrowserFrameActiveState active_state) const {
  return GetFrameThemeProvider()->GetColor(
      ShouldPaintAsActive(active_state)
          ? ThemeProperties::COLOR_FRAME_ACTIVE
          : ThemeProperties::COLOR_FRAME_INACTIVE);
}

void BrowserNonClientFrameView::UpdateFrameColor() {
  // Only web-app windows support dynamic frame colors set by HTML meta tags.
  if (web_app_frame_toolbar_)
    web_app_frame_toolbar_->UpdateCaptionColors();
  SchedulePaint();
}

SkColor BrowserNonClientFrameView::GetToolbarTopSeparatorColor() const {
  const int color_id =
      ShouldPaintAsActive()
          ? ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR
          : ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR_INACTIVE;
  // The vertical tab separator might show through the stroke if the stroke
  // color is translucent.  To prevent this, always use an opaque stroke color.
  return color_utils::GetResultingPaintColor(
      GetFrameThemeProvider()->GetColor(color_id), GetFrameColor());
}

base::Optional<int> BrowserNonClientFrameView::GetCustomBackgroundId(
    BrowserFrameActiveState active_state) const {
  const ui::ThemeProvider* tp = GetThemeProvider();
  const bool incognito = browser_view_->GetIncognito();
  const bool active = ShouldPaintAsActive(active_state);
  const int active_id =
      incognito ? IDR_THEME_TAB_BACKGROUND_INCOGNITO : IDR_THEME_TAB_BACKGROUND;
  const int inactive_id =
      incognito ? IDR_THEME_TAB_BACKGROUND_INCOGNITO_INACTIVE
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
  return has_custom_image ? base::make_optional(id) : base::nullopt;
}

void BrowserNonClientFrameView::UpdateMinimumSize() {}

void BrowserNonClientFrameView::Layout() {
  // BrowserView updates most UI visibility on layout based on fullscreen
  // state. However, it doesn't have access to |web_app_frame_toolbar_|. Do
  // it here. This is necessary since otherwise the visibility of ink drop
  // layers won't be updated; see crbug.com/964215.
  if (web_app_frame_toolbar_)
    web_app_frame_toolbar_->SetVisible(!frame_->IsFullscreen());

  NonClientFrameView::Layout();
}

void BrowserNonClientFrameView::VisibilityChanged(views::View* starting_from,
                                                  bool is_visible) {
  // UpdateTaskbarDecoration() calls DrawTaskbarDecoration(), but that does
  // nothing if the window is not visible.  So even if we've already gotten the
  // up-to-date decoration, we need to run the update procedure again here when
  // the window becomes visible.
  if (is_visible)
    OnProfileAvatarChanged(base::FilePath());
}

int BrowserNonClientFrameView::NonClientHitTest(const gfx::Point& point) {
  if (!web_app_frame_toolbar_)
    return HTNOWHERE;
  int web_app_component =
      views::GetHitTestComponent(web_app_frame_toolbar_, point);
  if (web_app_component != HTNOWHERE)
    return web_app_component;

  return HTNOWHERE;
}

void BrowserNonClientFrameView::ResetWindowControls() {
  if (web_app_frame_toolbar_)
    web_app_frame_toolbar_->UpdateStatusIconsVisibility();
}

void BrowserNonClientFrameView::PaintAsActiveChanged() {
  // The toolbar top separator color (used as the stroke around the tabs and
  // the new tab button) needs to be recalculated.
  browser_view_->tab_strip_region_view()->FrameColorsChanged();

  if (web_app_frame_toolbar_)
    web_app_frame_toolbar_->SetPaintAsActive(ShouldPaintAsActive());

  // Changing the activation state may change the visible frame color.
  SchedulePaint();
}

bool BrowserNonClientFrameView::ShouldPaintAsActive(
    BrowserFrameActiveState active_state) const {
  return (active_state == BrowserFrameActiveState::kUseCurrent)
             ? ShouldPaintAsActive()
             : (active_state == BrowserFrameActiveState::kActive);
}

gfx::ImageSkia BrowserNonClientFrameView::GetFrameImage(
    BrowserFrameActiveState active_state) const {
  const ui::ThemeProvider* tp = GetFrameThemeProvider();
  const int frame_image_id = ShouldPaintAsActive(active_state)
                                 ? IDR_THEME_FRAME
                                 : IDR_THEME_FRAME_INACTIVE;
  return (tp->HasCustomImage(frame_image_id) ||
          tp->HasCustomImage(IDR_THEME_FRAME))
             ? *tp->GetImageSkiaNamed(frame_image_id)
             : gfx::ImageSkia();
}

gfx::ImageSkia BrowserNonClientFrameView::GetFrameOverlayImage(
    BrowserFrameActiveState active_state) const {
  if (browser_view_->GetIncognito() || !browser_view_->GetIsNormalType())
    return gfx::ImageSkia();

  const ui::ThemeProvider* tp = GetFrameThemeProvider();
  const int frame_overlay_image_id = ShouldPaintAsActive(active_state)
                                         ? IDR_THEME_FRAME_OVERLAY
                                         : IDR_THEME_FRAME_OVERLAY_INACTIVE;
  return tp->HasCustomImage(frame_overlay_image_id)
             ? *tp->GetImageSkiaNamed(frame_overlay_image_id)
             : gfx::ImageSkia();
}

void BrowserNonClientFrameView::ChildPreferredSizeChanged(views::View* child) {
  if (browser_view()->initialized() && child == web_app_frame_toolbar_)
    Layout();
}

bool BrowserNonClientFrameView::DoesIntersectRect(const views::View* target,
                                                  const gfx::Rect& rect) const {
  DCHECK_EQ(target, this);
  if (!views::ViewTargeterDelegate::DoesIntersectRect(this, rect)) {
    // |rect| is outside the frame's bounds.
    return false;
  }

  bool should_leave_to_top_container = false;
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // In immersive mode, the caption buttons container is reparented to the
  // TopContainerView and hence |rect| should not be claimed here.  See
  // BrowserNonClientFrameViewChromeOS::OnImmersiveRevealStarted().
  should_leave_to_top_container =
      browser_view_->immersive_mode_controller()->IsRevealed();
#endif

  if (!browser_view_->GetTabStripVisible()) {
    // Claim |rect| if it is above the top of the topmost client area view.
    return !should_leave_to_top_container && (rect.y() < GetTopInset(false));
  }

  // If the rect is outside the bounds of the client area, claim it.
  gfx::RectF rect_in_client_view_coords_f(rect);
  View::ConvertRectToTarget(this, frame_->client_view(),
                            &rect_in_client_view_coords_f);
  gfx::Rect rect_in_client_view_coords =
      gfx::ToEnclosingRect(rect_in_client_view_coords_f);
  if (!frame_->client_view()->HitTestRect(rect_in_client_view_coords))
    return true;

  // Otherwise, claim |rect| only if it is above the bottom of the tab strip
  // region view in a non-tab portion.
  TabStripRegionView* tab_strip_region_view =
      browser_view_->tab_strip_region_view();

  // The |tab_strip_region_view| may not be in a Widget (e.g. when switching
  // into immersive reveal the BrowserView's TopContainerView is reparented).
  if (tab_strip_region_view->GetWidget()) {
    gfx::RectF rect_in_region_view_coords_f(rect);
    View::ConvertRectToTarget(this, tab_strip_region_view,
                              &rect_in_region_view_coords_f);
    gfx::Rect rect_in_region_view_coords =
        gfx::ToEnclosingRect(rect_in_region_view_coords_f);
    if (rect_in_region_view_coords.y() >=
        tab_strip_region_view->GetLocalBounds().bottom()) {
      // |rect| is below the tab_strip_region_view.
      return false;
    }

    if (tab_strip_region_view->HitTestRect(rect_in_region_view_coords)) {
      // Claim |rect| if it is in a non-tab portion of the tabstrip.
      return tab_strip_region_view->IsRectInWindowCaption(
          rect_in_region_view_coords);
    }
  }

  // We claim |rect| because it is above the bottom of the tabstrip, but
  // not in the tabstrip itself.
  return !should_leave_to_top_container;
}

void BrowserNonClientFrameView::OnProfileAdded(
    const base::FilePath& profile_path) {
  OnProfileAvatarChanged(profile_path);
}

void BrowserNonClientFrameView::OnProfileWasRemoved(
    const base::FilePath& profile_path,
    const std::u16string& profile_name) {
  OnProfileAvatarChanged(profile_path);
}

void BrowserNonClientFrameView::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
#if defined(OS_WIN)
  taskbar::UpdateTaskbarDecoration(browser_view()->browser()->profile(),
                                   frame_->GetNativeWindow());
#endif
}

void BrowserNonClientFrameView::OnProfileHighResAvatarLoaded(
    const base::FilePath& profile_path) {
#if defined(OS_WIN)
  taskbar::UpdateTaskbarDecoration(browser_view()->browser()->profile(),
                                   frame_->GetNativeWindow());
#endif
}

#if defined(OS_WIN)
int BrowserNonClientFrameView::GetSystemMenuY() const {
  if (!browser_view()->GetTabStripVisible())
    return GetTopInset(false);
  return GetBoundsForTabStripRegion(
             browser_view()->tab_strip_region_view()->GetMinimumSize())
             .bottom() -
         GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP);
}
#endif

const ui::ThemeProvider* BrowserNonClientFrameView::GetFrameThemeProvider()
    const {
  // The |frame_| theme provider is obtained from the profile rather than the
  // widget. This is done this way because it can happen prior to being inserted
  // into the view hierarchy.
  return frame_->GetThemeProvider();
}

BEGIN_METADATA(BrowserNonClientFrameView, views::NonClientFrameView)
END_METADATA
