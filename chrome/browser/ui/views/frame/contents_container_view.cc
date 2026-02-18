// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/contents_container_view.h"

#include <memory>
#include <optional>

#include "chrome/browser/actor/ui/actor_overlay_web_view.h"
#include "chrome/browser/devtools/devtools_contents_resizing_strategy.h"
#include "chrome/browser/enterprise/watermark/watermark_view.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/read_anything/immersive_read_anything_overlay_view.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_capture_border_view.h"
#include "chrome/browser/ui/views/frame/contents_container_outline.h"
#include "chrome/browser/ui/views/frame/contents_separator.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_view_mini_toolbar.h"
#include "chrome/browser/ui/views/frame/scrim_view.h"
#include "chrome/browser/ui/views/frame/tab_modal_dialog_host.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/new_tab_footer/footer_web_view.h"
#include "chrome/common/chrome_features.h"
#include "components/search/ntp_features.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/border.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/browser_ui/context_sharing_border_view.h"
#include "chrome/browser/glic/browser_ui/context_sharing_border_view_controller_impl.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "ui/views/widget/native_widget_aura.h"
#endif

namespace {
constexpr float kContentCornerRadius = 6;
constexpr gfx::RoundedCornersF kContentRoundedCorners{kContentCornerRadius};
constexpr int kSplitViewContentPadding = 4;

constexpr int kNewTabFooterSeparatorHeight = 1;
constexpr int kNewTabFooterHeight = 56;
}  // namespace

ContentsContainerView::ContentsContainerView(BrowserView* browser_view)
    : browser_view_(browser_view),
      web_contents_modal_dialog_host_(browser_view_, this) {
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));
  SetProperty(views::kElementIdentifierKey, kContentsContainerViewElementId);

  // The default z-order is the order in which children were added to the
  // parent view. So first added devtools and the devtools scrim view (as it
  // exists behind the content view), then the content view and new tab page
  // footer. This should be followed by scrims, borders and lastly mini-toolbar.

  auto devtools_web_view =
      std::make_unique<views::WebView>(browser_view->GetProfile());
  devtools_web_view->SetID(VIEW_ID_DEV_TOOLS_DOCKED);
  devtools_web_view->SetVisible(false);
  devtools_web_view_ = AddChildView(std::move(devtools_web_view));

  devtools_scrim_view_ = AddChildView(std::make_unique<ScrimView>());
  devtools_scrim_view_->layer()->SetName("DevtoolsScrimView");

  contents_view_ = AddChildView(
      std::make_unique<ContentsWebView>(browser_view->GetProfile()));
  contents_view_->SetID(VIEW_ID_TAB_CONTAINER);

  if (base::FeatureList::IsEnabled(ntp_features::kNtpFooter)) {
    new_tab_footer_view_separator_ =
        AddChildView(ContentsSeparator::CreateContentsSeparator());
    new_tab_footer_view_separator_->SetVisible(false);
    new_tab_footer_view_separator_->SetProperty(
        views::kElementIdentifierKey, kFooterWebViewSeparatorElementId);

    new_tab_footer_view_ =
        AddChildView(std::make_unique<new_tab_footer::NewTabFooterWebView>(
            browser_view->browser()));
    new_tab_footer_view_->SetVisible(false);
  }

  watermark_view_ =
      AddChildView(std::make_unique<enterprise_watermark::WatermarkView>());

  if (features::IsImmersiveReadAnythingEnabled()) {
    auto immersive_read_anything_overlay_view =
        std::make_unique<ImmersiveReadAnythingOverlayView>();
    immersive_read_anything_overlay_view_ =
        AddChildView(std::move(immersive_read_anything_overlay_view));
  }

  contents_scrim_view_ = AddChildView(std::make_unique<ScrimView>());
  contents_scrim_view_->layer()->SetName("ContentsScrimView");

  if (features::kGlicActorUiOverlay.Get()) {
    auto actor_overlay_web_view =
        std::make_unique<ActorOverlayWebView>(browser_view->browser());
    actor_overlay_web_view->SetID(VIEW_ID_ACTOR_OVERLAY);
    actor_overlay_web_view_ = AddChildView(std::move(actor_overlay_web_view));
  }

#if BUILDFLAG(ENABLE_GLIC)
  if (glic::GlicEnabling::IsProfileEligible(browser_view->GetProfile())) {
    glic_border_ = AddChildView(
        views::Builder<glic::ContextSharingBorderView>(
            glic::ContextSharingBorderView::Factory::Create(
                std::make_unique<
                    glic::ContextSharingBorderViewControllerImpl>(),
                browser_view->browser(), contents_view_))
            .SetVisible(false)
            .SetCanProcessEventsWithinSubtree(false)
            .Build());
  }
#endif

  if (base::FeatureList::IsEnabled(features::kSideBySide)) {
    mini_toolbar_ = AddChildView(std::make_unique<MultiContentsViewMiniToolbar>(
        browser_view, contents_view_));

    container_outline_ =
        AddChildView(std::make_unique<ContentsContainerOutline>(mini_toolbar_));
  }

  view_bounds_observer_.Observe(contents_view_);
}

ContentsContainerView::~ContentsContainerView() = default;

std::vector<views::View*> ContentsContainerView::GetAccessiblePanes() {
  std::vector<views::View*> accessible_panes;
  if (contents_view_->GetVisible()) {
    accessible_panes.push_back(contents_view_);
  }
  if (devtools_web_view_->GetVisible()) {
    accessible_panes.push_back(devtools_web_view_);
  }
  if (devtools_scrim_view_->GetVisible()) {
    accessible_panes.push_back(devtools_scrim_view_);
  }
  return accessible_panes;
}

void ContentsContainerView::UpdateBorderAndOverlay(bool is_in_split,
                                                   bool is_active,
                                                   bool is_highlighted) {
  const bool split_changed = is_in_split != is_in_split_;
  is_in_split_ = is_in_split;

  if (!is_in_split) {
    if (split_changed) {
      SetBorder(nullptr);
      ClearBorderRoundedCorners();
      mini_toolbar_->SetVisible(false);
      container_outline_->SetVisible(false);
    }
  } else {
    if (split_changed) {
      SetBorder(views::CreateEmptyBorder(gfx::Insets(
          kSplitViewContentPadding + ContentsContainerOutline::kThickness)));
      UpdateBorderRoundedCorners();
    }

    container_outline_->UpdateState(is_active, is_highlighted);
    // Mini toolbar should only be visible for the inactive contents
    // container view or both depending on configuration.
    mini_toolbar_->UpdateState(is_active, is_highlighted);
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (split_changed) {
    // Ensures correct window rounded corners after updating contents rounded
    // corners in UpdateBorderRoundedCorners()/ClearBorderRoundedCorners().
    GetWidget()->non_client_view()->frame_view()->UpdateWindowRoundedCorners();
  }
#endif  //  BUILDFLAG(IS_CHROMEOS)
}

void ContentsContainerView::UpdateBorderRoundedCorners() {
  // Update devtools rounded corners. Note, devtools exists behind the contents
  // view so all devtools corners are rounded.
  devtools_web_view_->holder()->SetCornerRadii(kContentRoundedCorners);
  devtools_scrim_view_->SetRoundedCorners(kContentRoundedCorners);

  const bool devtools_in_upper_left =
      devtools_web_view_->GetVisible() &&
      current_devtools_docked_placement_ == DevToolsDockedPlacement::kLeft;
  const bool devtools_in_upper_right =
      devtools_web_view_->GetVisible() &&
      current_devtools_docked_placement_ == DevToolsDockedPlacement::kRight;
  const bool devtools_in_lower_left =
      devtools_web_view_->GetVisible() &&
      (current_devtools_docked_placement_ == DevToolsDockedPlacement::kBottom ||
       current_devtools_docked_placement_ == DevToolsDockedPlacement::kLeft);
  const bool devtools_in_lower_right =
      devtools_web_view_->GetVisible() &&
      (current_devtools_docked_placement_ == DevToolsDockedPlacement::kBottom ||
       current_devtools_docked_placement_ == DevToolsDockedPlacement::kRight);

  const gfx::RoundedCornersF content_upper_rounded_corners =
      gfx::RoundedCornersF{devtools_in_upper_left ? 0 : kContentCornerRadius,
                           devtools_in_upper_right ? 0 : kContentCornerRadius,
                           0, 0};
  const gfx::RoundedCornersF content_lower_rounded_corners =
      gfx::RoundedCornersF{0, 0,
                           devtools_in_lower_right ? 0 : kContentCornerRadius,
                           devtools_in_lower_left ? 0 : kContentCornerRadius};
  const gfx::RoundedCornersF content_rounded_corners =
      gfx::RoundedCornersF{devtools_in_upper_left ? 0 : kContentCornerRadius,
                           devtools_in_upper_right ? 0 : kContentCornerRadius,
                           devtools_in_lower_right ? 0 : kContentCornerRadius,
                           devtools_in_lower_left ? 0 : kContentCornerRadius};

  auto radii = new_tab_footer_view_ && new_tab_footer_view_->GetVisible()
                   ? content_upper_rounded_corners
                   : content_rounded_corners;

  contents_view_->SetBackgroundRadii(radii);
  contents_view_->holder()->SetCornerRadii(radii);
  contents_scrim_view_->SetRoundedCorners(kContentRoundedCorners);

  if (new_tab_footer_view_) {
    new_tab_footer_view_->holder()->SetCornerRadii(
        content_lower_rounded_corners);
  }

  if (actor_overlay_web_view_) {
    // ActorOverlayWebView should use the same radii as the contents view since
    // it acts as a full transparent layer directly over the main web content.
    actor_overlay_web_view_->holder()->SetCornerRadii(radii);
  }

#if BUILDFLAG(ENABLE_GLIC)
  if (glic_border_) {
    glic_border_->SetRoundedCorners(content_rounded_corners);
  }
#endif
}

void ContentsContainerView::ClearBorderRoundedCorners() {
  constexpr gfx::RoundedCornersF kNoRoundedCorners = gfx::RoundedCornersF{0};

  devtools_web_view_->holder()->SetCornerRadii(kNoRoundedCorners);
  devtools_scrim_view_->SetRoundedCorners(kNoRoundedCorners);

  contents_view_->SetBackgroundRadii(kNoRoundedCorners);
  contents_view_->holder()->SetCornerRadii(kNoRoundedCorners);

  if (new_tab_footer_view_) {
    new_tab_footer_view_->holder()->SetCornerRadii(kNoRoundedCorners);
  }

  contents_scrim_view_->SetRoundedCorners(kNoRoundedCorners);

  if (actor_overlay_web_view_) {
    actor_overlay_web_view_->holder()->SetCornerRadii(kNoRoundedCorners);
  }

#if BUILDFLAG(ENABLE_GLIC)
  if (glic_border_) {
    glic_border_->SetRoundedCorners(kNoRoundedCorners);
  }
#endif
}

void ContentsContainerView::ChildVisibilityChanged(View* child) {
  if ((child == new_tab_footer_view_ || child == devtools_web_view_) &&
      is_in_split_) {
    UpdateBorderRoundedCorners();
  }
}

void ContentsContainerView::Layout(PassKey pass_key) {
  LayoutSuperclass<views::View>(this);

  if (capture_contents_border_widget_) {
    UpdateCaptureContentsBorderLocation();
  }
}

void ContentsContainerView::OnViewBoundsChanged(View* observed_view) {
  if (observed_view == contents_view_) {
    UpdateDevToolsDockedPlacement();
    if (is_in_split_) {
      UpdateBorderRoundedCorners();
    }
  }
}

void ContentsContainerView::SetContentsResizingStrategy(
    const DevToolsContentsResizingStrategy& strategy) {
  if (strategy_.Equals(strategy)) {
    return;
  }

  strategy_.CopyFrom(strategy);
  InvalidateLayout();
}

void ContentsContainerView::ApplyWatermarkSettings(
    const std::string& watermark_text,
    SkColor fill_color,
    SkColor outline_color,
    int font_size) {
  watermark_view_->SetString(watermark_text, fill_color, outline_color,
                             font_size);
}

void ContentsContainerView::UpdateDevToolsDockedPlacement() {
  DevToolsDockedPlacement placement = DevToolsDockedPlacement::kUnknown;
  gfx::Rect contents_view_bounds = GetContentsViewBounds();
  const gfx::Rect& container_bounds = GetContentsBounds();

  // If contents_webview has the same bounds as webview_container, it either
  // means that devtools are not open or devtools are open in a separate
  // window (not docked).
  if (contents_view_bounds == container_bounds) {
    placement = DevToolsDockedPlacement::kNone;
  } else if (contents_view_bounds.x() > container_bounds.x() &&
             contents_view_bounds.y() == container_bounds.y() &&
             contents_view_bounds.height() == container_bounds.height()) {
    placement = DevToolsDockedPlacement::kLeft;
  } else if (contents_view_bounds.origin() == container_bounds.origin() &&
             contents_view_bounds.height() == container_bounds.height()) {
    placement = DevToolsDockedPlacement::kRight;
  } else if (contents_view_bounds.origin() == container_bounds.origin() &&
             contents_view_bounds.width() == container_bounds.width()) {
    placement = DevToolsDockedPlacement::kBottom;
  }

  // When browser window is resizing, the contents_container and web_contents
  // bounds can be out of sync, resulting in a state, where it is impossible to
  // infer docked placement based on contents webview bounds. In this case, use
  // the last known docked placement, since resizing a window does not change
  // the devtools dock placement.
  if (placement != DevToolsDockedPlacement::kUnknown) {
    current_devtools_docked_placement_ = placement;
  }
}

void ContentsContainerView::ShowCaptureContentsBorder() {
  if (!capture_contents_border_widget_) {
    CreateCaptureContentsBorder();
  }

  UpdateCaptureContentsBorderLocation();
  capture_contents_border_widget_->Show();
}

void ContentsContainerView::HideCaptureContentsBorder() {
  if (capture_contents_border_widget_) {
    capture_contents_border_widget_->Hide();
  }
}

void ContentsContainerView::SetCaptureContentsBorderLocation(
    std::optional<gfx::Rect> border_location) {
  dynamic_capture_content_border_bounds_ = border_location;
  if (capture_contents_border_widget_) {
    UpdateCaptureContentsBorderLocation();
  }
}

gfx::Rect ContentsContainerView::GetContentsViewBounds() const {
  gfx::Rect contents_view_bounds = contents_view_->bounds();
  if (new_tab_footer_view_ && new_tab_footer_view_->GetVisible()) {
    CHECK(new_tab_footer_view_separator_);
    contents_view_bounds.set_height(contents_view_bounds.height() +
                                    new_tab_footer_view_->height() +
                                    new_tab_footer_view_separator_->height());
  }

  return contents_view_bounds;
}

void ContentsContainerView::CreateCaptureContentsBorder() {
  capture_contents_border_widget_ = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  views::Widget* widget = GetWidget();
  params.parent = widget->GetNativeView();
  params.context = widget->GetNativeWindow();
  // Make the widget non-top level.
  params.child = true;
  params.name = "TabSharingContentsBorder";
  params.remove_standard_frame = true;
  // Let events go through to underlying view.
  params.accept_events = false;
  params.activatable = views::Widget::InitParams::Activatable::kNo;
#if BUILDFLAG(IS_WIN)
  params.native_widget =
      new views::NativeWidgetAura(capture_contents_border_widget_.get());
#endif  // BUILDFLAG(IS_WIN)

  capture_contents_border_widget_->Init(std::move(params));
  auto contents_capture_border_view =
      std::make_unique<ContentsCaptureBorderView>();
  capture_contents_border_widget_->SetContentsView(
      std::move(contents_capture_border_view));
  capture_contents_border_widget_->SetVisibilityChangedAnimationsEnabled(false);
  capture_contents_border_widget_->SetOpacity(0.50f);
}

void ContentsContainerView::UpdateCaptureContentsBorderLocation() {
  gfx::Point contents_top_left;
#if BUILDFLAG(IS_CHROMEOS)
  // On Ash placing the border widget on top of the contents container
  // does not require an offset -- see crbug.com/1030925.
  const gfx::Rect bounds_in_browser =
      views::View::ConvertRectToTarget(this, browser_view_, GetLocalBounds());
  contents_top_left = gfx::Point(bounds_in_browser.x(), bounds_in_browser.y());
#else
  views::View::ConvertPointToScreen(this, &contents_top_left);
#endif
  gfx::Rect rect;
  if (dynamic_capture_content_border_bounds_) {
    rect = gfx::Rect(
        contents_top_left.x() + dynamic_capture_content_border_bounds_->x(),
        contents_top_left.y() + dynamic_capture_content_border_bounds_->y(),
        dynamic_capture_content_border_bounds_->width(),
        dynamic_capture_content_border_bounds_->height());
  } else {
    rect = gfx::Rect(contents_top_left.x(), contents_top_left.y(), width(),
                     height());
  }

#if BUILDFLAG(IS_CHROMEOS)
  // Immersive top container might overlap with the blue border in fullscreen
  // mode - see crbug.com/1392733. By insetting the bounds rectangle we ensure
  // that the blue border is always placed below the top container.
  if (ImmersiveModeController::From(browser_view_->browser())->IsRevealed()) {
    const int delta =
        browser_view_->top_container()->bounds().bottom() - rect.y();
    if (delta > 0) {
      rect.Inset(gfx::Insets().set_top(delta));
    }
  }
#endif

#if BUILDFLAG(IS_MAC)
  // Zero sized widgets are not supported on mac.
  if (rect.IsEmpty()) {
    return;
  }
#endif  // BUILDFLAG(IS_MAC)

  capture_contents_border_widget_->SetBounds(rect);
}

views::ProposedLayout ContentsContainerView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  if (!size_bounds.is_fully_bounded()) {
    return layouts;
  }

  int height = size_bounds.height().value();
  int width = size_bounds.width().value();

  if (width == 0 || height == 0) {
    // On Wayland we receive a resize to 0 width first before the actual
    // size bounds. Ignore such requests.
    return layouts;
  }

  gfx::Rect full_contents_bounds = GetContentsBounds();
  gfx::Rect devtools_bounds;
  // The area contents excluding devtools is drawn (ie |contents_view_|,
  // |new_tab_footer_view_|, etc).
  gfx::Rect non_devtools_contents_bounds;

  ApplyDevToolsContentsResizingStrategy(strategy_, full_contents_bounds,
                                        &devtools_bounds,
                                        &non_devtools_contents_bounds);
  gfx::Rect contents_view_bounds = non_devtools_contents_bounds;

  // DevTools cares about the specific position, so we have to compensate RTL
  // layout here.
  layouts.child_layouts.emplace_back(
      devtools_web_view_.get(), devtools_web_view_->GetVisible(),
      GetMirroredRect(devtools_bounds),
      views::SizeBounds(full_contents_bounds.size()));
  layouts.child_layouts.emplace_back(
      devtools_scrim_view_.get(), devtools_scrim_view_->GetVisible(),
      GetMirroredRect(devtools_bounds),
      views::SizeBounds(full_contents_bounds.size()));

  if (new_tab_footer_view_) {
    gfx::Rect footer_separator_rect, footer_rect;
    if (new_tab_footer_view_->GetVisible()) {
      // Shrink the rect for the contents view if the ntp footer is visible.
      contents_view_bounds.set_height(non_devtools_contents_bounds.height() -
                                      kNewTabFooterHeight -
                                      kNewTabFooterSeparatorHeight);
      footer_separator_rect =
          gfx::Rect(contents_view_bounds.x(), contents_view_bounds.bottom(),
                    contents_view_bounds.width(), kNewTabFooterSeparatorHeight);
      footer_rect =
          gfx::Rect(contents_view_bounds.x(), contents_view_bounds.bottom(),
                    contents_view_bounds.width(), kNewTabFooterHeight);
    }

    layouts.child_layouts.emplace_back(new_tab_footer_view_separator_.get(),
                                       new_tab_footer_view_->GetVisible(),
                                       footer_separator_rect);
    layouts.child_layouts.emplace_back(new_tab_footer_view_.get(),
                                       new_tab_footer_view_->GetVisible(),
                                       footer_rect);
  }

  const auto& contents_rect = GetMirroredRect(contents_view_bounds);
  layouts.child_layouts.emplace_back(
      contents_view_.get(), contents_view_->GetVisible(), contents_rect);

#if BUILDFLAG(ENABLE_GLIC)
  if (glic_border_) {
    // |glic_border_| should not be seen over devtools.
    layouts.child_layouts.emplace_back(glic_border_.get(),
                                       glic_border_->GetVisible(),
                                       non_devtools_contents_bounds);
  }
#endif

  // The content scrim view should cover the entire contents bounds.
  CHECK(contents_scrim_view_);
  layouts.child_layouts.emplace_back(contents_scrim_view_.get(),
                                     contents_scrim_view_->GetVisible(),
                                     full_contents_bounds);

  CHECK(watermark_view_);
  layouts.child_layouts.emplace_back(watermark_view_.get(),
                                     watermark_view_->GetVisible(),
                                     full_contents_bounds);

  // Actor Overlay view bounds are the same as the contents view.
  if (actor_overlay_web_view_) {
    layouts.child_layouts.emplace_back(
        actor_overlay_web_view_.get(), actor_overlay_web_view_->GetVisible(),
        non_devtools_contents_bounds, size_bounds);
  }

  // Reading Mode overlay view bounds are the same as the contents view.
  if (features::IsImmersiveReadAnythingEnabled() &&
      immersive_read_anything_overlay_view_) {
    layouts.child_layouts.emplace_back(
        immersive_read_anything_overlay_view_.get(),
        immersive_read_anything_overlay_view_->GetVisible(),
        non_devtools_contents_bounds, size_bounds);
  }

  if (mini_toolbar_) {
    // |mini_toolbar_| should be offset in the bottom right corner, overlapping
    // the outline. Shrink the available space by corner radius to ensure we
    // have space to draw it at the corners.
    views::SizeBounds available_space(width, height);
    available_space.Enlarge(-ContentsContainerOutline::kCornerRadius,
                            -ContentsContainerOutline::kCornerRadius);
    gfx::Size mini_toolbar_size =
        mini_toolbar_->GetPreferredSize(available_space);
    const int offset_x = width - mini_toolbar_size.width();
    const int offset_y = height - mini_toolbar_size.height();
    const gfx::Rect mini_toolbar_rect =
        gfx::Rect(offset_x, offset_y, mini_toolbar_size.width(),
                  mini_toolbar_size.height());
    layouts.child_layouts.emplace_back(
        mini_toolbar_.get(), mini_toolbar_->GetVisible(), mini_toolbar_rect);
  }

  if (container_outline_) {
    layouts.child_layouts.emplace_back(container_outline_.get(),
                                       container_outline_->GetVisible(),
                                       gfx::Rect(0, 0, width, height));
  }

  layouts.host_size = gfx::Size(width, height);
  return layouts;
}

BEGIN_METADATA(ContentsContainerView)
END_METADATA
