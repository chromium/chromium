// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/layout/browser_view_layout_impl.h"

#include <map>
#include <optional>

#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_delegate.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_params.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/proposed_layout.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/fullscreen_util_mac.h"
#endif

namespace {

// Shorthand for validating both `child` and `parent` and checking that one is
// parented to the other. Ignores child visibility.
bool IsParentedTo(const views::View* child, const views::View* parent) {
  return child && parent && child->parent() == parent;
}

// Shorthand for validating both `child` and `parent` and checking that one is
// parented to the other. If `child` is not visible, returns false.
bool IsParentedToAndVisible(const views::View* child,
                            const views::View* parent) {
  return IsParentedTo(child, parent) && child->GetVisible();
}

// Insets `span` by `amount` on either the `leading` or (if false) trailing
// edge, to a minimum of zero width.
void Inset(views::Span& span, int amount, bool leading) {
  if (leading) {
    const int old_end = span.end();
    span.set_start(std::min(span.end(), span.start() + amount));
    span.set_length(old_end - span.start());
  } else {
    span.set_end(std::max(span.start(), span.end() - amount));
  }
}

// Gets the bounds for a `view`, placed between the exclusion zones in `params`
// if they are present.
gfx::Rect GetBoundsWithExclusion(const BrowserLayoutParams& params,
                                 const views::View* view,
                                 int leading_margin = 0,
                                 int trailing_margin = 0) {
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

}  // namespace

struct BrowserViewLayoutImpl::ProposedLayout {
  ProposedLayout(const gfx::Rect& bounds_, std::optional<bool> visibility_)
      : bounds(bounds_), visibility(visibility_) {}
  ProposedLayout() = default;
  ~ProposedLayout() = default;

  // Current view's bounds relative to its parent.
  gfx::Rect bounds;

  // If visibility is to be set during layout, set this flag.
  std::optional<bool> visibility;

  // Layouts of children of the current view.
  //
  // It is very important that this object not be stored, but only exist on the
  // stack during calls, as the `raw_ptr` may otherwise dangle.
  std::map<raw_ptr<views::View, CtnExperimental>, ProposedLayout> children;

  // Adds a child layout for `child` and returns the layout. Fails if `child` is
  // already present.
  ProposedLayout& AddChild(views::View* child,
                           const gfx::Rect& bounds_,
                           std::optional<bool> visibility_ = std::nullopt) {
    const auto emplace_result =
        children.emplace(child, ProposedLayout(bounds_, visibility_));
    CHECK(emplace_result.second)
        << "Already added layout for " << child->GetClassName();
    return emplace_result.first->second;
  }

  // Searches the tree for `descendant` and returns its layout, otherwise,
  // returns null if not found.
  const ProposedLayout* GetLayoutFor(const views::View* descendant) const {
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

  // Finds `descendant`'s layout in the tree and returns its bounds relative to
  // `relative_to`.
  std::optional<gfx::Rect> GetBoundsFor(const views::View* descendant,
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

  using SetViewVisibility =
      base::FunctionRef<void(views::View* view, bool visible)>;

  // Applies this layout to `root`. In order to ensure that all child layouts
  // are applied, this is an inherently destructive operation; each child layout
  // is removed as it is applied and if there are any orphan layouts a stack
  // dump is triggered (this will be a CHECK() in the future).
  void ApplyLayout(views::View* root,
                   SetViewVisibility set_view_visibility) && {
    for (auto& child : root->children()) {
      if (const auto it = children.find(child); it != children.end()) {
        if (it->second.visibility) {
          set_view_visibility(child, *it->second.visibility);
        }
        child->SetBoundsRect(it->second.bounds);
        std::move(it->second).ApplyLayout(child, set_view_visibility);
        children.erase(it);
      }
    }
    if (!children.empty()) {
      const views::View* const leftover = children.begin()->first;
      DUMP_WILL_BE_NOTREACHED()
          << "Unapplied layout remains for " << leftover->GetClassName()
          << " in " << root->GetClassName();
    }
  }
};

BrowserViewLayoutImpl::BrowserViewLayoutImpl(
    std::unique_ptr<BrowserViewLayoutDelegate> delegate,
    Browser* browser,
    BrowserViewLayoutViews views)
    : BrowserViewLayout(std::move(delegate), browser, std::move(views)) {}

BrowserViewLayoutImpl::~BrowserViewLayoutImpl() = default;

void BrowserViewLayoutImpl::Layout(views::View* host) {
  const auto params = delegate().GetBrowserLayoutParams();
  if (params.IsEmpty()) {
    return;
  }
  CalculateProposedLayout(params).ApplyLayout(
      host, [this](views::View* view, bool visible) {
        SetViewVisibility(view, visible);
      });

  MaybeLayoutTopContainerOverlay(params);
}

void BrowserViewLayoutImpl::MaybeLayoutTopContainerOverlay(
    const BrowserLayoutParams& params) {
  // There are probably cases where `params` require some translation, but for
  // now, just use them as-is. Also determine which platforms require
  // exclusions and which do not.

  // If the top container is parented to the main container, it is not in the
  // overlay.
  if (!views().top_container ||
      views().top_container->parent() == views().main_container) {
    return;
  }

  // In slide/immersive mode, animating the top container is handled by someone
  // else, but there are adjustments that are needed to be made.
  ProposedLayout top_container_layout;

  // The computation for the top container components does not change.
  gfx::Rect top_container_bounds =
      CalculateTopContainerLayout(top_container_layout, params, true);

  // Position the top container in its parent, whatever that is.
  views().top_container->SetBoundsRect(top_container_bounds);

  // Apply the child layouts for the top container.
  std::move(top_container_layout)
      .ApplyLayout(views().top_container,
                   [this](views::View* view, bool visible) {
                     SetViewVisibility(view, visible);
                   });
}

bool BrowserViewLayoutImpl::ContentsSeparatorInTopContainer() const {
  // If there is no multi-contents view, there's nowhere else to put the
  // separator, so it goes in the top container.
  if (!views().multi_contents_view) {
    return true;
  }

  // In immersive mode, when the top container is visually separate, the
  // separator goes with the container to the overlay.
  bool top_container_is_visually_separate =
      delegate().GetImmersiveModeController()->IsEnabled();
#if BUILDFLAG(IS_MAC)
  // On Mac, when in full browser fullscreen (but not content fullscreen), the
  // entire top container is always visible and does not look like an
  // immersive mode overlay, so in this case the top container isn't visually
  // separate from the browser.
  if (top_container_is_visually_separate &&
      fullscreen_utils::IsAlwaysShowToolbarEnabled(browser()) &&
      !fullscreen_utils::IsInContentFullscreen(browser())) {
    top_container_is_visually_separate = false;
  }
#endif
  if (top_container_is_visually_separate) {
    return true;
  }

  // If the infobar is visible, the separator has to go in the top container.
  if (IsInfobarVisible()) {
    return true;
  }

  // The separator should go in the multi contents view instead.
  return false;
}

gfx::Size BrowserViewLayoutImpl::GetMinimumSize(const views::View* host) const {
  // This is a simplified version of the same method in
  // `BrowserViewLayoutImplOld` that assumes a standard browser.
  const gfx::Size tabstrip_size =
      views().tab_strip_region_view->GetMinimumSize();
  const gfx::Size toolbar_size = views().toolbar->GetMinimumSize();
  const gfx::Size bookmark_bar_size =
      (views().bookmark_bar && views().bookmark_bar->GetVisible())
          ? views().bookmark_bar->GetMinimumSize()
          : gfx::Size();
  const gfx::Size infobar_container_size =
      views().infobar_container->GetMinimumSize();
  const gfx::Size contents_size = views().contents_container->GetMinimumSize();
  const gfx::Size contents_height_side_panel_size =
      views().contents_height_side_panel &&
              views().contents_height_side_panel->GetVisible()
          ? views().contents_height_side_panel->GetMinimumSize()
          : gfx::Size();

  const int min_height =
      tabstrip_size.height() + toolbar_size.height() +
      bookmark_bar_size.height() + infobar_container_size.height() +
      std::max({contents_size.height(),
                contents_height_side_panel_size.height(), 1});

  // TODO(https://crbug.com/454583671): This probably needs to be more
  // sophisticated to handle separators, etc. but it's unwieldy to do it without
  // better decomposition of the layout.
  const int min_width = std::max(
      {tabstrip_size.width(), toolbar_size.width(), bookmark_bar_size.width(),
       infobar_container_size.width(),
       (contents_size.width() + contents_height_side_panel_size.width()),
       kMainBrowserContentsMinimumWidth});

  return gfx::Size(min_width, min_height);
}

int BrowserViewLayoutImpl::GetMinWebContentsWidthForTesting() const {
  return kMainBrowserContentsMinimumWidth;
}

BrowserViewLayoutImpl::ProposedLayout
BrowserViewLayoutImpl::CalculateProposedLayout(
    const BrowserLayoutParams& params) const {
  // TODO(https://crbug.com/453717426): Consider caching layouts of the same
  // size if no `InvalidateLayout()` has happened.

  // Build the proposed layout here:
  TRACE_EVENT0("ui", "BrowserViewLayoutImpl::CalculateProposedLayout");
  ProposedLayout layout;

  // The window scrim covers the entire browser view.
  if (views().window_scrim) {
    layout.AddChild(views().window_scrim, params.visual_client_area);
  }

  // TODO(https://crbug.com/453717426): Handle vertical tabstrip here.

  int y = params.visual_client_area.y();
  bool used_exclusion = false;

  // Lay out tab strip region.
  if (IsParentedTo(views().tab_strip_region_view, views().browser_view)) {
    gfx::Rect tabstrip_bounds;
    const bool tabstrip_visible = delegate().ShouldDrawTabStrip();
    if (tabstrip_visible) {
      // Inset the leading edge of the tabstrip by the size of the swoop of the
      // first tab; this is especially important for Mac, where the negative
      // space of the caption button margins and the edge of the tabstrip should
      // overlap. The trailing edge receives the usual treatment, as it is the
      // new tab button and not a tab.
      tabstrip_bounds =
          GetBoundsWithExclusion(params, views().tab_strip_region_view,
                                 TabStyle::Get()->GetBottomCornerRadius());
      // TODO(https://crbug.com/454583671): Figure out if we always want to
      // apply TABSTRIP_TOOLBAR_OVERLAP, or whether it should not apply to
      // toolbar height side panel.
      y = tabstrip_bounds.bottom() -
          GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP);
      used_exclusion = true;
    }
    layout.AddChild(views().tab_strip_region_view, tabstrip_bounds,
                    tabstrip_visible);
  }

  int x = params.visual_client_area.x();

  // The insets for main region and its containing views when the
  // toolbar_height_side_panel is visible.
  const int container_inset_padding =
      GetLayoutConstant(LayoutConstant::TOOLBAR_HEIGHT_SIDE_PANEL_INSET) +
      views::Separator::kThickness;

  // Lay out toolbar-height side panel.
  if (IsParentedToAndVisible(views().toolbar_height_side_panel,
                             views().browser_view)) {
    const gfx::Rect main_background_region_bounds(
        x, std::max(y, params.visual_client_area.y()),
        params.visual_client_area.width(),
        params.visual_client_area.bottom() - params.visual_client_area.y());
    layout.AddChild(views().main_background_region,
                    main_background_region_bounds, true);

    const int width =
        views().toolbar_height_side_panel->GetPreferredSize().width();
    const int visible_width = base::ClampFloor(
        width * views().toolbar_height_side_panel->GetAnimationValue());
    // Add container_inset_padding to the top of the toolbar height side panel
    // to separate it from the tab strip. SidePanel draws the top on top of the
    // top content separator and some units of the toolbar by default, which is
    // not needed for the toolbar height side panel.
    const int top = std::max(
        y + container_inset_padding,
        params.visual_client_area.y() +
            base::ClampCeil(
                params.leading_exclusion.ContentWithPadding().height()));
    gfx::Rect toolbar_height_bounds(x - (width - visible_width), top, width,
                                    params.visual_client_area.bottom() - top);
    x = toolbar_height_bounds.right();
    layout.AddChild(views().toolbar_height_side_panel, toolbar_height_bounds);
  } else {
    // Set the main_background_region bounds to 0 since it should only be
    // visible when toolbar height side panel is visible.
    layout.AddChild(views().main_background_region, gfx::Rect(), false);
  }

  // Layout the main container.
  gfx::Rect main_bounds(x, y, params.visual_client_area.width() - x,
                        params.visual_client_area.height() - y);

  if (IsParentedToAndVisible(views().toolbar_height_side_panel,
                             views().browser_view)) {
    // When the toolbar height side panel is visible, main container is shifted
    // and separated by container_inset_padding on all side. This includes
    // padding the top of main_container from the tab_strip.
    main_bounds.Inset(container_inset_padding);
  }

  const BrowserLayoutParams main_params =
      params.InLocalCoordinates(main_bounds);
  ProposedLayout& main_layout =
      layout.AddChild(views().main_container, main_bounds);
  CalculateMainContainerLayout(main_layout, main_params, !used_exclusion);

  return layout;
}

void BrowserViewLayoutImpl::CalculateMainContainerLayout(
    ProposedLayout& layout,
    const BrowserLayoutParams& params,
    bool needs_exclusion) const {
  int y = params.visual_client_area.y();

  // Lay out top container.
  if (IsParentedToAndVisible(views().top_container, views().main_container)) {
    // Take advantage of the fact that the top container takes up the entire top
    // area of the main container.
    ProposedLayout& top_container_layout =
        layout.AddChild(views().top_container, gfx::Rect());
    top_container_layout.bounds = CalculateTopContainerLayout(
        top_container_layout, params, needs_exclusion);
    y = top_container_layout.bounds.bottom();
  }

  // TODO(https://crbug.com/7089871): handle "toolbar always visible" mode.

  // Lay out infobar container.
  if (IsParentedTo(views().infobar_container, views().main_container)) {
    gfx::Rect infobar_bounds;
    const bool infobar_visible = delegate().IsInfobarVisible();
    if (infobar_visible) {
      infobar_bounds = gfx::Rect(
          params.visual_client_area.x(),
          // Infobar needs to get down out of the way of immersive mode elements
          // in some cases.
          y + delegate().GetImmersiveModeController()->GetExtraInfobarOffset(),
          params.visual_client_area.width(),
          // This returns zero for empty infobar.
          views().infobar_container->GetPreferredSize().height());
      y = infobar_bounds.bottom();
    }
    layout.AddChild(views().infobar_container, infobar_bounds, infobar_visible);
  }

  // Lay out contents-height side panel.
  views::Span horizontal_space(params.visual_client_area.x(),
                               params.visual_client_area.width());
  bool show_left_separator = false;
  bool show_right_separator = false;
  bool side_panel_leading = false;
  int min_contents_width = kMainBrowserContentsMinimumWidth;

  // The contents-height side panel is adjusted for the presence of a top
  // container separator in the browser view.
  const auto* top_separator_layout =
      layout.GetLayoutFor(views().top_container_separator);
  const int side_panel_top =
      top_separator_layout && top_separator_layout->visibility.value()
          ? y - views::Separator::kThickness
          : y;

  if (IsParentedTo(views().contents_height_side_panel,
                   views().main_container)) {
    SidePanel* const side_panel = views().contents_height_side_panel;
    int side_panel_width = 0;
    int side_panel_visible_width = 0;
    const bool is_right_aligned = side_panel->IsRightAligned();
    side_panel_leading = is_right_aligned == base::i18n::IsRTL();
    if (side_panel->GetVisible()) {
      // Side panel implies a separator, which means we have to give a little
      // more room for the contents.
      min_contents_width += views::Separator::kThickness;
      show_left_separator = !is_right_aligned;
      show_right_separator = is_right_aligned;

      // Maximum width is the lesser of preferred width and the largest width
      // that doesn't shrink the contents pane past its own minimum size.
      const int min_width = side_panel->GetMinimumSize().width();
      const int preferred_width = side_panel->GetPreferredSize().width();
      int max_width = std::min(preferred_width,
                               horizontal_space.length() - min_contents_width);
      if (side_panel->ShouldRestrictMaxWidth()) {
        max_width = std::min(max_width, horizontal_space.length() * 2 / 3);
      }

      // Side panel always gets at least its minimum width.
      side_panel_width = std::max(min_width, max_width);
      side_panel_visible_width = base::ClampFloor(
          side_panel_width *
          views().contents_height_side_panel->GetAnimationValue());
    }

    // Side panel slides in from the edge of the main container..
    const gfx::Rect side_panel_bounds(
        side_panel_leading ? horizontal_space.start() -
                                 (side_panel_width - side_panel_visible_width)
                           : horizontal_space.end() - side_panel_visible_width,
        side_panel_top, side_panel_width,
        params.visual_client_area.bottom() - side_panel_top);
    layout.AddChild(side_panel, side_panel_bounds);
    Inset(horizontal_space, side_panel_visible_width, side_panel_leading);
  }

  // This will be used to position the separator corner.
  const int separator_edge =
      side_panel_leading ? horizontal_space.start() : horizontal_space.end();

  // Maybe show separators in multi-contents view. If this happens, the
  // separators aren't shown in the main container. Note that the multi-contents
  // view is inside the main container so doesn't need to be laid out.
  if (views().multi_contents_view) {
    bool show_leading_separator = false;
    bool show_trailing_separator = false;
    if (show_left_separator || show_right_separator) {
      show_leading_separator = side_panel_leading;
      show_trailing_separator = !side_panel_leading;
    }
    views().multi_contents_view->SetShouldShowLeadingSeparator(
        show_leading_separator);
    views().multi_contents_view->SetShouldShowTrailingSeparator(
        show_trailing_separator);
    show_left_separator = false;
    show_right_separator = false;
  }

  // Lay out the left side panel separator.
  if (IsParentedTo(views().left_aligned_side_panel_separator,
                   views().main_container)) {
    gfx::Rect separator_bounds;
    if (show_left_separator) {
      const int separator_width =
          views().left_aligned_side_panel_separator->GetPreferredSize().width();
      separator_bounds = gfx::Rect(
          side_panel_leading ? horizontal_space.start()
                             : horizontal_space.end() - separator_width,
          y, separator_width, params.visual_client_area.bottom() - y);
      Inset(horizontal_space, separator_width, side_panel_leading);
    }
    layout.AddChild(views().left_aligned_side_panel_separator, separator_bounds,
                    show_left_separator);
  }

  // Lay out the right side panel separator.
  if (IsParentedTo(views().right_aligned_side_panel_separator,
                   views().main_container)) {
    gfx::Rect separator_bounds;
    if (show_right_separator) {
      const int separator_width =
          views()
              .right_aligned_side_panel_separator->GetPreferredSize()
              .width();
      separator_bounds = gfx::Rect(
          side_panel_leading ? horizontal_space.start()
                             : horizontal_space.end() - separator_width,
          y, separator_width, params.visual_client_area.bottom() - y);
      Inset(horizontal_space, separator_width, side_panel_leading);
    }
    layout.AddChild(views().right_aligned_side_panel_separator,
                    separator_bounds, show_right_separator);
  }

  // Lay out the corner separator.
  if (IsParentedTo(views().side_panel_rounded_corner, views().main_container)) {
    const bool visible = show_left_separator || show_right_separator;
    gfx::Rect corner_bounds;
    if (visible) {
      const gfx::Size corner_size =
          views().side_panel_rounded_corner->GetPreferredSize();
      const gfx::Point corner_pos(side_panel_leading
                                      ? separator_edge
                                      : separator_edge - corner_size.width(),
                                  side_panel_top);
      corner_bounds = gfx::Rect(corner_pos, corner_size);
    }
    layout.AddChild(views().side_panel_rounded_corner, corner_bounds, visible);
  }

  // Lay out contents container. The contents container contains the multi-
  // contents view when multi-contents are enabled. The checks here are to
  // force the logic to be updated when multi-contents is fully rolled-out.
  CHECK(IsParentedToAndVisible(views().contents_container,
                               views().main_container));
  CHECK(views().multi_contents_view == nullptr ||
        views().contents_container->Contains(views().multi_contents_view));

  // Because side panels have minimum width, in a small browser, it is possible
  // for the combination of minimum-sized contents pane and minimum-sized side
  // panel may exceed the width of the window. In this case, the contents pane
  // slides under the side panel.
  if (const int deficit = min_contents_width - horizontal_space.length();
      deficit > 0) {
    // Expand the contents by the deficit on the side with the side panel.
    Inset(horizontal_space, -deficit, side_panel_leading);
  }
  layout.AddChild(
      views().contents_container,
      gfx::Rect(horizontal_space.start(), y, horizontal_space.length(),
                params.visual_client_area.bottom() - y));
}

gfx::Rect BrowserViewLayoutImpl::CalculateTopContainerLayout(
    ProposedLayout& layout,
    const BrowserLayoutParams& params,
    bool needs_exclusion) const {
  int y = params.visual_client_area.y();

  // If the tabstrip is in the top container (which can happen in immersive
  // mode), ensure it is laid out here.
  if (IsParentedTo(views().tab_strip_region_view, views().top_container)) {
    gfx::Rect tabstrip_bounds;
    const bool tabstrip_visible = delegate().ShouldDrawTabStrip();
    if (tabstrip_visible) {
      // When there is an exclusion, inset the leading edge of the tabstrip by
      // the size of the swoop of the first tab; this is especially important
      // for Mac, where the negative space of the caption button margins and the
      // edge of the tabstrip should overlap. The trailing edge receives the
      // usual treatment, as it is the new tab button and not a tab.
      tabstrip_bounds =
          needs_exclusion
              ? GetBoundsWithExclusion(params, views().tab_strip_region_view,
                                       TabStyle::Get()->GetBottomCornerRadius())
              : gfx::Rect(
                    params.visual_client_area.x(), y,
                    params.visual_client_area.width(),
                    views().tab_strip_region_view->GetPreferredSize().height());
      y = tabstrip_bounds.bottom();
      needs_exclusion = false;
    }
    layout.AddChild(views().tab_strip_region_view, tabstrip_bounds,
                    tabstrip_visible);
  }

  // Lay out toolbar. If tabstrip is completely absent (or vertical), this can
  // go in the top exclusion area.
  const bool toolbar_visible = delegate().IsToolbarVisible();
  if (IsParentedTo(views().toolbar, views().top_container)) {
    gfx::Rect toolbar_bounds;
    if (toolbar_visible) {
      toolbar_bounds =
          needs_exclusion
              ? GetBoundsWithExclusion(params, views().toolbar)
              : gfx::Rect(params.visual_client_area.x(), y,
                          params.visual_client_area.width(),
                          views().toolbar->GetPreferredSize().height());
      y = toolbar_bounds.bottom();
      needs_exclusion = false;
    }
    layout.AddChild(views().toolbar, toolbar_bounds, toolbar_visible);
  }

  // Lay out the bookmarks bar if one is present.
  const bool bookmarks_visible = delegate().IsBookmarkBarVisible();
  if (IsParentedTo(views().bookmark_bar, views().top_container)) {
    const gfx::Rect bookmarks_bounds(
        params.visual_client_area.x(), y, params.visual_client_area.width(),
        bookmarks_visible ? views().bookmark_bar->GetPreferredSize().height()
                          : 0);
    layout.AddChild(views().bookmark_bar, bookmarks_bounds, bookmarks_visible);
    y = bookmarks_bounds.bottom();
  }

  // The top separator may need to be shown in the top container or the
  // multi-contents view. It is shown when the toolbar or bookmarks are present
  // in the top container.
  const bool show_top_separator = toolbar_visible || bookmarks_visible;
  const bool separator_in_top_container =
      show_top_separator && ContentsSeparatorInTopContainer();

  // Maybe show the separator in the multi-contents view. If this happens, it
  // does not appear in the top container.
  if (views().multi_contents_view) {
    views().multi_contents_view->SetShouldShowTopSeparator(
        show_top_separator && !separator_in_top_container);
  }

  // Maybe show the separator in the top container.
  if (IsParentedTo(views().top_container_separator, views().top_container)) {
    gfx::Rect separator_bounds;
    if (separator_in_top_container) {
      separator_bounds = gfx::Rect(
          params.visual_client_area.x(), y, params.visual_client_area.width(),
          views().top_container_separator->GetPreferredSize().height());
      y = separator_bounds.bottom();
    }
    layout.AddChild(views().top_container_separator, separator_bounds,
                    separator_in_top_container);
  }

  // In certain circumstances, the top container bounds require adjustment.
  int top = params.visual_client_area.y();
  const int height = y - params.visual_client_area.y();

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
            gfx::Size(params.visual_client_area.width(), height));
  }

  // These are the bounds for the top container.
  return gfx::Rect(params.visual_client_area.x(), top,
                   params.visual_client_area.width(), height);
}

// Dialog positioning.

int BrowserViewLayoutImpl::GetDialogTop(const ProposedLayout& layout) const {
  const int kConstrainedWindowOverlap = 3;
  const auto* const browser_view = views().browser_view.get();
  if (const auto toolbar_rect =
          layout.GetBoundsFor(views().toolbar, browser_view)) {
    return toolbar_rect->bottom() - kConstrainedWindowOverlap;
  }
  if (const auto webapp_toolbar_rect =
          layout.GetBoundsFor(views().web_app_frame_toolbar, browser_view)) {
    return webapp_toolbar_rect->bottom() - kConstrainedWindowOverlap;
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

views::Span BrowserViewLayoutImpl::GetDialogHorizontalTarget(
    const ProposedLayout& layout) const {
  const auto* const browser_view = views().browser_view.get();
  views::Span horizontal;
  if (const auto contents_rect =
          layout.GetBoundsFor(views().contents_container, browser_view)) {
    horizontal.set_start(contents_rect->x());
    horizontal.set_length(contents_rect->width());
  } else {
    horizontal.set_end(browser_view->width());
  }
  return horizontal;
}

gfx::Point BrowserViewLayoutImpl::GetDialogPosition(
    const gfx::Size& dialog_size) const {
  const auto params = delegate().GetBrowserLayoutParams();
  if (params.IsEmpty()) {
    return gfx::Point();
  }
  const ProposedLayout layout = CalculateProposedLayout(params);
  const auto horizontal = GetDialogHorizontalTarget(layout);
  return gfx::Point(horizontal.start() + horizontal.length() / 2,
                    GetDialogTop(layout));
}

gfx::Size BrowserViewLayoutImpl::GetMaximumDialogSize() const {
  const auto params = delegate().GetBrowserLayoutParams();
  if (params.IsEmpty()) {
    return gfx::Size();
  }
  const ProposedLayout layout = CalculateProposedLayout(params);
  const auto horizontal = GetDialogHorizontalTarget(layout);
  const int top = GetDialogTop(layout);
  const int bottom = GetDialogBottom(layout);
  return gfx::Size(horizontal.length(), bottom - top);
}
