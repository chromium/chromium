// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/contents_container_view.h"

#include <memory>
#include <optional>

#include "chrome/browser/actor/ui/actor_overlay_web_view.h"
#include "chrome/browser/devtools/devtools_contents_resizing_strategy.h"
#include "chrome/browser/enterprise/watermark/watermark_view.h"
#include "chrome/browser/glic/browser_ui/context_sharing_border_view.h"
#include "chrome/browser/glic/browser_ui/context_sharing_border_view_controller_impl.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ai_overlay_dialog/ai_overlay_dialog_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/read_anything/read_anything_immersive_overlay_view.h"
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
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

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

  if (base::FeatureList::IsEnabled(features::kAiOverlayDialog)) {
    auto ai_overlay_dialog_view =
        std::make_unique<views::WebView>(browser_view->GetProfile());
    ai_overlay_dialog_view->SetVisible(false);
    ai_overlay_dialog_view->SetProperty(views::kElementIdentifierKey,
                                        kAiOverlayDialogWebViewElementId);
    ai_overlay_dialog_view->EnableSizingFromWebContents(gfx::Size(1, 1),
                                                        gfx::Size(800, 600));
    ai_overlay_dialog_view_ = AddChildView(std::move(ai_overlay_dialog_view));
  }

  if (features::IsImmersiveReadAnythingEnabled()) {
    auto read_anything_immersive_overlay_view =
        std::make_unique<ReadAnythingImmersiveOverlayView>(contents_view_);
    read_anything_immersive_overlay_view_ =
        AddChildView(std::move(read_anything_immersive_overlay_view));
  }

  contents_scrim_view_ = AddChildView(std::make_unique<ScrimView>());
  contents_scrim_view_->layer()->SetName("ContentsScrimView");

  if (base::FeatureList::IsEnabled(features::kGlicActorUi) &&
      features::kGlicActorUiOverlay.Get()) {
    auto actor_overlay_web_view =
        std::make_unique<ActorOverlayWebView>(browser_view->browser());
    actor_overlay_web_view->SetID(VIEW_ID_ACTOR_OVERLAY);
    actor_overlay_web_view_ = AddChildView(std::move(actor_overlay_web_view));
  }

  if (base::FeatureList::IsEnabled(features::kGlicRegionSelectionNew)) {
    auto glic_selection_overlay_view = std::make_unique<views::WebView>();
    glic_selection_overlay_view->SetProperty(
        views::kElementIdentifierKey, kGlicSelectionOverlayViewElementId);
    glic_selection_overlay_view->SetVisible(false);
    glic_selection_overlay_view->SetLayoutManager(
        std::make_unique<views::FillLayout>());
    glic_selection_overlay_view_ =
        AddChildView(std::move(glic_selection_overlay_view));
  }

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

  mini_toolbar_ = AddChildView(std::make_unique<MultiContentsViewMiniToolbar>(
      browser_view, contents_view_));

  container_outline_ =
      AddChildView(std::make_unique<ContentsContainerOutline>(mini_toolbar_));

  capture_contents_border_view_ =
      AddChildView(std::make_unique<ContentsCaptureBorderView>(mini_toolbar_));

  view_bounds_observer_.Observe(contents_view_);
}

ContentsContainerView::~ContentsContainerView() {
  // read_anything_immersive_overlay_view_ holds a raw_ptr to
  // contents_view_. We need to make sure we destroy
  // read_anything_immersive_overlay_view_ first to avoid a dangling pointer.
  if (read_anything_immersive_overlay_view_) {
    auto overlay_view = RemoveChildViewT(read_anything_immersive_overlay_view_);
    read_anything_immersive_overlay_view_ = nullptr;
  }
}

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
      if (capture_contents_border_view_) {
        capture_contents_border_view_->SetIsInSplit(false);
      }
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
    if (capture_contents_border_view_) {
      capture_contents_border_view_->SetIsInSplit(true);
    }
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

  if (ai_overlay_dialog_view_) {
    // ai_overlay_dialog_view_ should use the same radii as the contents view
    // since it acts as a layer directly over the main web content.
    ai_overlay_dialog_view_->holder()->SetCornerRadii(radii);
  }

  if (glic_selection_overlay_view_) {
    glic_selection_overlay_view_->holder()->SetCornerRadii(radii);
  }

  if (glic_border_) {
    glic_border_->SetRoundedCorners(content_rounded_corners);
  }
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

  if (ai_overlay_dialog_view_) {
    ai_overlay_dialog_view_->holder()->SetCornerRadii(kNoRoundedCorners);
  }

  if (glic_selection_overlay_view_) {
    glic_selection_overlay_view_->holder()->SetCornerRadii(kNoRoundedCorners);
  }

  if (glic_border_) {
    glic_border_->SetRoundedCorners(kNoRoundedCorners);
  }
}

void ContentsContainerView::ChildVisibilityChanged(View* child) {
  if ((child == new_tab_footer_view_ || child == devtools_web_view_) &&
      is_in_split_) {
    UpdateBorderRoundedCorners();
  }
}

void ContentsContainerView::Layout(PassKey pass_key) {
  LayoutSuperclass<views::View>(this);

  UpdateContentsClip();
}

views::View::Views ContentsContainerView::GetChildrenInZOrder() {
#if DCHECK_IS_ON()
  auto ordered_children = views::View::GetChildrenInZOrder();
  // |capture_contents_border_view_| should have the highest z-order.
  DCHECK(ordered_children.back() == capture_contents_border_view_);
  return ordered_children;
#else
  return views::View::GetChildrenInZOrder();
#endif
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
  if (capture_contents_border_view_) {
    capture_contents_border_view_->SetVisible(true);
  }
}

void ContentsContainerView::HideCaptureContentsBorder() {
  if (capture_contents_border_view_) {
    capture_contents_border_view_->SetVisible(false);
  }
}

void ContentsContainerView::SetCaptureContentsBorderLocation(
    std::optional<gfx::Rect> border_location) {
  if (capture_contents_border_view_) {
    capture_contents_border_view_->SetCaptureContentsBorderLocation(
        border_location);
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

void ContentsContainerView::SetTargetContentBounds(
    std::optional<gfx::Outsets> target_content_bounds) {
  if (target_content_bounds_ == target_content_bounds) {
    return;
  }

  target_content_bounds_ = target_content_bounds;
  InvalidateLayout(/*avoid_propagate_during_layout=*/true);
}

void ContentsContainerView::UpdateContentsClip() {
  bool changed = false;
  if (auto* const layer = contents_view_->holder()->GetUILayer()) {
    if (layer->clip_rect() != contents_clip_rect_) {
      layer->SetClipRect(contents_clip_rect_);
      changed = true;
    }
  }
  if (auto* const layer = contents_view_->layer()) {
    if (layer->clip_rect() != contents_clip_rect_) {
      layer->SetClipRect(contents_clip_rect_);
      changed = true;
    }
  }
  if (changed) {
    contents_view_->SchedulePaint();
  }
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

  if (glic_border_) {
    // |glic_border_| should not be seen over devtools.
    layouts.child_layouts.emplace_back(glic_border_.get(),
                                       glic_border_->GetVisible(),
                                       non_devtools_contents_bounds);
  }

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

  if (ai_overlay_dialog_view_) {
    // TODO(b/490458384): Look into whether the view can be transparent to hit
    // testing (in transparent parts) - otherwise autosize it to the inner web
    // content.
    gfx::Size size = ai_overlay_dialog_view_->GetPreferredSize();
    if (size.IsEmpty()) {
      int dialog_width = 200;
      int dialog_height = 200;
      if (!features::kAiOverlayDialogMockJsonPath.Get().empty()) {
        // 150px (buttons) + 20px (gap) + 100px (persona) = 270px
        dialog_width = 270;
        // 200px (max height of column)
        dialog_height = 200;
      }
      size = gfx::Size(dialog_width, dialog_height);
    }
    int x_margin = 15;
    gfx::Point top_left = non_devtools_contents_bounds.bottom_right() -
                          gfx::Vector2d(size.width() + x_margin, size.height());
    gfx::Rect rect(top_left, size);
    layouts.child_layouts.emplace_back(ai_overlay_dialog_view_.get(),
                                       ai_overlay_dialog_view_->GetVisible(),
                                       rect, views::SizeBounds(rect.size()));
  }

  if (glic_selection_overlay_view_) {
    layouts.child_layouts.emplace_back(
        glic_selection_overlay_view_.get(),
        glic_selection_overlay_view_->GetVisible(),
        non_devtools_contents_bounds, size_bounds);
  }

  // Reading Mode overlay view bounds are the same as the contents view.
  if (features::IsImmersiveReadAnythingEnabled() &&
      read_anything_immersive_overlay_view_) {
    layouts.child_layouts.emplace_back(
        read_anything_immersive_overlay_view_.get(),
        read_anything_immersive_overlay_view_->GetVisible(),
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

  if (capture_contents_border_view_) {
    gfx::Rect rect;
    if (auto capture_location =
            capture_contents_border_view_->capture_location();
        capture_location) {
      rect = *capture_location;
      rect.Offset(contents_view_bounds.OffsetFromOrigin());
    } else {
      rect = contents_view_bounds;
    }

#if BUILDFLAG(IS_CHROMEOS)
    // Immersive top container might overlap with the blue border in fullscreen
    // mode - see crbug.com/40880524. By insetting the bounds rectangle we
    // ensure that the blue border is always placed below the top container.
    if (ImmersiveModeController::From(browser_view_->browser())->IsRevealed()) {
      const int delta =
          browser_view_->top_container()->bounds().bottom() - rect.y();
      if (delta > 0) {
        rect.Inset(gfx::Insets().set_top(delta));
      }
    }
#endif

    bool visible = capture_contents_border_view_->GetVisible();
#if BUILDFLAG(IS_MAC)
    // Zero sized view should not be shown.
    if (rect.IsEmpty()) {
      visible = false;
    }
#endif  // BUILDFLAG(IS_MAC)

    layouts.child_layouts.emplace_back(capture_contents_border_view_.get(),
                                       visible, rect,
                                       views::SizeBounds(rect.size()));
  }

  auto* const content_layout = layouts.GetLayoutFor(contents_view_);
  if (target_content_bounds_) {
    content_layout->bounds.Outset(*target_content_bounds_);
    contents_clip_rect_ =
        gfx::Rect(gfx::Point(), content_layout->bounds.size());
    contents_clip_rect_.Inset(-target_content_bounds_->ToInsets());
  } else {
    contents_clip_rect_ = gfx::Rect();
  }

  layouts.host_size = gfx::Size(width, height);
  return layouts;
}

BEGIN_METADATA(ContentsContainerView)
END_METADATA
