// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/layout/browser_view_layout_impl.h"

#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_delegate.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/view.h"

// Proposed layout implementation.

BrowserViewLayoutImpl::ProposedLayout::ProposedLayout(
    const gfx::Rect& bounds_,
    std::optional<bool> visibility_)
    : bounds(bounds_), visibility(visibility_) {}
BrowserViewLayoutImpl::ProposedLayout::ProposedLayout() = default;
BrowserViewLayoutImpl::ProposedLayout::ProposedLayout(
    ProposedLayout&&) noexcept = default;
BrowserViewLayoutImpl::ProposedLayout&
BrowserViewLayoutImpl::ProposedLayout::operator=(ProposedLayout&&) noexcept =
    default;
BrowserViewLayoutImpl::ProposedLayout::~ProposedLayout() = default;

BrowserViewLayoutImpl::ProposedLayout&
BrowserViewLayoutImpl::ProposedLayout::AddChild(
    views::View* child,
    const gfx::Rect& bounds_,
    std::optional<bool> visibility_) {
  const auto emplace_result =
      children.emplace(child, ProposedLayout(bounds_, visibility_));
  CHECK(emplace_result.second)
      << "Already added layout for " << child->GetClassName();
  return emplace_result.first->second;
}

const BrowserViewLayoutImpl::ProposedLayout*
BrowserViewLayoutImpl::ProposedLayout::GetLayoutFor(
    const views::View* descendant) const {
  for (const auto& child : children) {
    if (child.first == descendant) {
      return &child.second;
    }
    if (auto* const result = child.second.GetLayoutFor(descendant)) {
      return result;
    }
  }
  return nullptr;
}

std::optional<gfx::Rect> BrowserViewLayoutImpl::ProposedLayout::GetBoundsFor(
    const views::View* descendant,
    const views::View* relative_to) const {
  const ProposedLayout* layout = GetLayoutFor(descendant);
  if (!layout) {
    return std::nullopt;
  }
  // Since layout bounds are relative to the parent, do the conversion from
  // there.
  return views::View::ConvertRectToTarget(descendant->parent(), relative_to,
                                          layout->bounds);
}

void BrowserViewLayoutImpl::ProposedLayout::ApplyLayout(
    views::View* root,
    SetViewVisibility set_view_visibility) && {
  for (auto& child : root->children()) {
    if (const auto it = children.find(child); it != children.end()) {
      // Need to tail-recurse here because otherwise, when we set the bounds of
      // the immediate child, this will automatically trigger a layout on all
      // of its children, which have not been properly arranged yet. This
      // results in a potential double-layout, or in extreme cases, bugs like
      // https://crbug.com/464220949
      std::move(it->second).ApplyLayout(child, set_view_visibility);
      child->SetBoundsRect(it->second.bounds);
      if (it->second.visibility) {
        set_view_visibility(child, *it->second.visibility);
      }
      children.erase(it);
    }
  }
  if (!children.empty()) {
    const views::View* const leftover = children.begin()->first;
    DUMP_WILL_BE_NOTREACHED()
        << "Unapplied layout remains for " << leftover->GetClassName() << " in "
        << root->GetClassName();
  }
}

// Common layout.

BrowserViewLayoutImpl::BrowserViewLayoutImpl(
    std::unique_ptr<BrowserViewLayoutDelegate> delegate,
    Browser* browser,
    BrowserViewLayoutViews views)
    : BrowserViewLayout(std::move(delegate), browser, std::move(views)) {}

BrowserViewLayoutImpl::~BrowserViewLayoutImpl() = default;

// Static helpers.

// static
bool BrowserViewLayoutImpl::IsParentedTo(const views::View* child,
                                         const views::View* parent) {
  return child && parent && child->parent() == parent;
}

// static
bool BrowserViewLayoutImpl::IsParentedToAndVisible(const views::View* child,
                                                   const views::View* parent) {
  return IsParentedTo(child, parent) && child->GetVisible();
}

// static
gfx::Rect BrowserViewLayoutImpl::GetBoundsWithExclusion(
    const BrowserLayoutParams& params,
    const views::View* view,
    int leading_margin,
    int trailing_margin) {
  const auto leading =
      leading_margin ? params.leading_exclusion.ContentWithPaddingAndInsets(
                           leading_margin, 0.f)
                     : params.leading_exclusion.ContentWithPadding();
  const auto trailing =
      trailing_margin ? params.trailing_exclusion.ContentWithPaddingAndInsets(
                            trailing_margin, 0.f)
                      : params.trailing_exclusion.ContentWithPadding();
  int height = base::ClampCeil(std::max(leading.height(), trailing.height()));
  if (height) {
    height = std::max(height, view->GetMinimumSize().height());
  } else {
    height = view->GetPreferredSize().height();
  }
  return gfx::Rect(
      /*x=*/params.visual_client_area.x() + leading.width(),
      /*y=*/params.visual_client_area.y(),
      /*width=*/params.visual_client_area.width() -
          (leading.width() + trailing.width()),
      /*height=*/height);
}

gfx::Rect BrowserViewLayoutImpl::GetTopContainerBoundsInParent(
    const gfx::Rect& local_bounds,
    const BrowserLayoutParams& parent_params) const {
  // Calculate the dimensions of the container.
  int top = local_bounds.y();
  const int height = local_bounds.height();
  if (height <= 0) {
    // Use an empty bounds.
    return gfx::Rect(parent_params.visual_client_area.origin(),
                     gfx::Size(local_bounds.width(), 0));
  }

  // In certain circumstances, the top container bounds require adjustment.
  if (delegate().IsTopControlsSlideBehaviorEnabled()) {
    // In slide mode, if the top container is hidden completely, it is placed
    // outside the window bounds.
    top =
        delegate().GetTopControlsSlideBehaviorShownRatio() == 0.0 ? -height : 0;
  } else if (auto* const controller = delegate().GetImmersiveModeController();
             controller && controller->IsEnabled()) {
    // If the immersive mode controller is animating the top container overlay,
    // it may be partly offscreen. The controller knows where the container
    // needs to be.
    top =
        delegate().GetImmersiveModeController()->GetTopContainerVerticalOffset(
            gfx::Size(parent_params.visual_client_area.width(), height));
  }

  gfx::Rect bounds = local_bounds;
  bounds.set_y(top);
  bounds.Offset(parent_params.visual_client_area.OffsetFromOrigin());
  return bounds;
}

int BrowserViewLayoutImpl::GetMinWebContentsWidthForTesting() const {
  return kContentsContainerMinimumWidth;
}

// Layout logic.

void BrowserViewLayoutImpl::Layout(views::View* host) {
  const auto params =
      delegate().GetBrowserLayoutParams(/*use_browser_bounds=*/true);
  if (params.IsEmpty()) {
    return;
  }

  // Lay out the browser view itself.
  CalculateProposedLayout(params).ApplyLayout(
      host, [this](views::View* view, bool visible) {
        SetViewVisibility(view, visible);
      });

  // If the top container is not parented to the main container, it is an
  // overlay and must be laid out separately.
  if (views().top_container &&
      views().top_container->parent() != views().browser_view) {
    // In slide/immersive mode, animating the top container is handled by
    // someone else, but there are adjustments that are needed to be made.
    ProposedLayout top_container_layout;

    // The computation for the top container components does not change.
    const gfx::Rect top_container_local_bounds = CalculateTopContainerLayout(
        top_container_layout, params, /*needs_exclusion=*/true);

    // Position the top container in its parent, whatever that is.
    views().top_container->SetBoundsRect(
        GetTopContainerBoundsInParent(top_container_local_bounds, params));

    // Apply the child layouts for the top container.
    std::move(top_container_layout)
        .ApplyLayout(views().top_container,
                     [this](views::View* view, bool visible) {
                       SetViewVisibility(view, visible);
                     });
  }

  // The normal clipping created by `View::Paint()` may not cover the bottom of
  // the TopContainerView at certain scale factor because both of the position
  // and the height might be rounded down. This function sets the clip path that
  // enlarges the height at 2 DPs to compensate this error (both origin and
  // size) that the canvas can cover the entire TopContainerView.  See
  // crbug.com/390669712 for more details.
  //
  // TODO(crbug.com/41344902): Remove this hack once the pixel canvas is enabled
  // on all aura platforms.  Note that macOS supports integer scale only, so
  // this isn't necessary on macOS.
  if (!features::IsPixelCanvasRecordingEnabled()) {
    const auto apply_bottom_paint_allowance = [](views::View* view) {
      constexpr int kBottomPaintAllowance = 2;
      view->SetClipPath(SkPath::Rect(SkRect::MakeWH(
          view->width(), view->height() + kBottomPaintAllowance)));
    };

    // Here are the views which require adjustment (add/remove as necessary).
    if (views().toolbar && views().toolbar->GetVisible()) {
      apply_bottom_paint_allowance(views().toolbar);
    }
    if (views().bookmark_bar && views().bookmark_bar->GetVisible()) {
      apply_bottom_paint_allowance(views().bookmark_bar);
    }
    apply_bottom_paint_allowance(views().top_container);
  }

  // Do any additional adjustments required by the specific layout.
  DoPostLayoutVisualAdjustments();

  // Update bubbles (like the find bar).
  UpdateBubbles();
}

// Dialog positioning.

int BrowserViewLayoutImpl::GetDialogTop(const ProposedLayout& layout) const {
  const int kConstrainedWindowOverlap = 3;
  const auto* const browser_view = views().browser_view.get();
  if (const auto toolbar_rect =
          layout.GetBoundsFor(views().toolbar, browser_view)) {
    return toolbar_rect->bottom() - kConstrainedWindowOverlap;
  }
  return kConstrainedWindowOverlap;
}

int BrowserViewLayoutImpl::GetDialogBottom(const ProposedLayout& layout) const {
  const auto* const browser_view = views().browser_view.get();
  if (const auto contents_rect =
          layout.GetBoundsFor(views().contents_container, browser_view)) {
    return contents_rect->bottom();
  }
  return browser_view->height();
}

gfx::Point BrowserViewLayoutImpl::GetDialogPosition(
    const gfx::Size& dialog_size) const {
  const auto params =
      delegate().GetBrowserLayoutParams(/*use_browser_bounds=*/false);
  if (params.IsEmpty()) {
    return gfx::Point();
  }
  const ProposedLayout layout = CalculateProposedLayout(params);

  // Calculate the dialog bounds in browser view space.
  const int browser_width = params.visual_client_area.width();
  const int dialog_x =
      params.visual_client_area.x() + (browser_width - dialog_size.width()) / 2;
  const int dialog_y = GetDialogTop(layout);
  gfx::Rect dialog_rect(dialog_x, dialog_y, dialog_size.width(),
                        dialog_size.height());

  // Convert to widget coordinates.
  dialog_rect = views().browser_view->ConvertRectToWidget(dialog_rect);

  // TODO: consider whether this should change in RTL?
  return gfx::Point(dialog_rect.origin());
}

gfx::Size BrowserViewLayoutImpl::GetMaximumDialogSize() const {
  const auto params =
      delegate().GetBrowserLayoutParams(/*use_browser_bounds=*/false);
  if (params.IsEmpty()) {
    return gfx::Size();
  }
  const ProposedLayout layout = CalculateProposedLayout(params);

  // This computation is irrespective of coordinate system (all coordinates
  // happen to be in browser view space).
  const int top = GetDialogTop(layout);
  const int bottom = GetDialogBottom(layout);
  return gfx::Size(params.visual_client_area.width(), bottom - top);
}
