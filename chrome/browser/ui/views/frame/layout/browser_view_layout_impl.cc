// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/layout/browser_view_layout_impl.h"

#include <map>
#include <optional>
#include <utility>

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
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/proposed_layout.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/fullscreen_util_mac.h"
#endif

namespace {

// The minimum width of the contents area itself. Applies even when side panels
// are open and prevents zero or negative contents sizes.
static constexpr int kContentsContainerMinimumWidth = 200;

// Loading bar is thicker than a separator, but instead of moving the bottom of
// the top container down, it starts above where the separator would go.
static constexpr int kLoadingBarHeight = 3;
static constexpr int kLoadingBarOffset =
    kLoadingBarHeight - views::Separator::kThickness;

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

void SetTop(BrowserLayoutParams& params, int top) {
  params.Inset(gfx::Insets::TLBR(top - params.visual_client_area.y(), 0, 0, 0));
}

// Insets `params` by `amount` on either the `leading` or (if false) trailing
// edge, to a minimum of zero width.
void InsetHorizontal(BrowserLayoutParams& params, int amount, bool leading) {
  amount = std::min(amount, params.visual_client_area.width());
  params.Inset(leading ? gfx::Insets::TLBR(0, amount, 0, 0)
                       : gfx::Insets::TLBR(0, 0, 0, amount));
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
  DoPostLayoutVisualAdjustments();
}

void BrowserViewLayoutImpl::MaybeLayoutTopContainerOverlay(
    const BrowserLayoutParams& params) {
  // There are probably cases where `params` require some translation, but for
  // now, just use them as-is. Also determine which platforms require
  // exclusions and which do not.

  // If the top container is parented to the main container, it is not in the
  // overlay.
  if (!views().top_container ||
      views().top_container->parent() == views().browser_view) {
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

void BrowserViewLayoutImpl::DoPostLayoutVisualAdjustments() {
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

  if (features::IsPixelCanvasRecordingEnabled()) {
    return;
  }

  const auto apply_bottom_paint_allowance = [](views::View* view) {
    constexpr int kBottomPaintAllowance = 2;
    view->SetClipPath(SkPath::Rect(
        SkRect::MakeWH(view->width(), view->height() + kBottomPaintAllowance)));
  };

  // Here are the views which require adjustment (add/remove as necessary).
  apply_bottom_paint_allowance(views().toolbar);
  if (views().bookmark_bar && views().bookmark_bar->GetVisible()) {
    apply_bottom_paint_allowance(views().bookmark_bar);
  }
  apply_bottom_paint_allowance(views().top_container);
}

BrowserViewLayoutImpl::TopSeparatorType
BrowserViewLayoutImpl::GetTopSeparatorType() const {
  if (!delegate().IsToolbarVisible() && !delegate().IsBookmarkBarVisible()) {
    return TopSeparatorType::kNone;
  }

  if (IsParentedTo(views().loading_bar, views().top_container)) {
    return TopSeparatorType::kLoadingBar;
  }

  // If there is no multi-contents view, there's nowhere else to put the
  // separator, so it goes in the top container.
  if (!views().multi_contents_view) {
    return TopSeparatorType::kTopContainer;
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
    return TopSeparatorType::kTopContainer;
  }

  // If the infobar is visible, the separator has to go in the top container.
  if (IsInfobarVisible()) {
    return TopSeparatorType::kTopContainer;
  }

  // The separator should go in the multi contents view instead.
  return TopSeparatorType::kMultiContents;
}

std::pair<gfx::Size, gfx::Size> BrowserViewLayoutImpl::GetMinimumTabStripSize()
    const {
  switch (GetTabStripType()) {
    case TabStripType::kHorizontal:
      return std::make_pair(gfx::Size(),
                            views().tab_strip_region_view->GetMinimumSize());
    case TabStripType::kVertical: {
      auto result = views().vertical_tab_strip_container->GetMinimumSize();
      result.set_width(std::max(result.width(), kMinVerticalTabStripWidth));
      return std::make_pair(result, gfx::Size());
    }
    case TabStripType::kWebUi:
      return std::make_pair(gfx::Size(),
                            views().webui_tab_strip->GetMinimumSize());
    case TabStripType::kNone:
      return std::make_pair(gfx::Size(), gfx::Size());
  }
}

gfx::Size BrowserViewLayoutImpl::GetMinimumMainAreaSize() const {
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

  const int width = std::max({toolbar_size.width(), bookmark_bar_size.width(),
                              infobar_container_size.width(),
                              contents_height_side_panel_size.width() +
                                  kContentsContainerMinimumWidth});
  const int height = toolbar_size.height() + bookmark_bar_size.height() +
                     infobar_container_size.height() +
                     std::max(contents_size.height(),
                              contents_height_side_panel_size.height());
  return gfx::Size(width, height);
}

BrowserViewLayoutImpl::TabStripType BrowserViewLayoutImpl::GetTabStripType()
    const {
  if (views().webui_tab_strip && views().webui_tab_strip->GetVisible()) {
    return TabStripType::kWebUi;
  }
  if (delegate().ShouldDrawVerticalTabStrip()) {
    return TabStripType::kVertical;
  }
  return delegate().ShouldDrawTabStrip() ? TabStripType::kHorizontal
                                         : TabStripType::kNone;
}

gfx::Size BrowserViewLayoutImpl::GetMinimumSize(const views::View* host) const {
  // This is a simplified version of the same method in
  // `BrowserViewLayoutImplOld` that assumes a standard browser.
  const auto [vertical_tabstrip_size, horizontal_tabstrip_size] =
      GetMinimumTabStripSize();
  const gfx::Size toolbar_height_side_panel_size =
      views().toolbar_height_side_panel &&
              views().toolbar_height_side_panel->GetVisible()
          ? views().toolbar_height_side_panel->GetMinimumSize()
          : gfx::Size();
  const gfx::Size main_area_size = GetMinimumMainAreaSize();

  int min_height =
      horizontal_tabstrip_size.height() +
      std::max({toolbar_height_side_panel_size.height(),
                main_area_size.height(), vertical_tabstrip_size.height()});

  // This assumes a horizontal tabstrip. There is also a hard minimum on the
  // width of the browser defined by `kMainBrowserContentsMinimumWidth`.
  int min_width =
      vertical_tabstrip_size.width() +
      std::max({horizontal_tabstrip_size.width(),
                toolbar_height_side_panel_size.width() + main_area_size.width(),
                kMainBrowserContentsMinimumWidth});

  // Maybe adjust for additional padding when toolbar height side panel is
  // visible.
  if (!toolbar_height_side_panel_size.IsEmpty()) {
    const auto padding =
        GetLayoutConstant(LayoutConstant::TOOLBAR_HEIGHT_SIDE_PANEL_INSET);
    min_height += 2 * padding;
    min_width += padding;
  }

  return gfx::Size(min_width, min_height);
}

int BrowserViewLayoutImpl::GetMinWebContentsWidthForTesting() const {
  return kContentsContainerMinimumWidth;
}

BrowserViewLayoutImpl::ProposedLayout
BrowserViewLayoutImpl::CalculateProposedLayout(
    const BrowserLayoutParams& browser_params) const {
  // TODO(https://crbug.com/453717426): Consider caching layouts of the same
  // size if no `InvalidateLayout()` has happened.

  // Build the proposed layout here:
  TRACE_EVENT0("ui", "BrowserViewLayoutImpl::CalculateProposedLayout");
  ProposedLayout layout;

  // The window scrim covers the entire browser view.
  if (views().window_scrim) {
    layout.AddChild(views().window_scrim, browser_params.visual_client_area);
  }

  BrowserLayoutParams params = browser_params;
  bool needs_exclusion = true;
  const TabStripType tab_strip_type = GetTabStripType();

  // Lay out WebUI tabstrip if visible.
  if (IsParentedTo(views().webui_tab_strip, views().browser_view)) {
    const int width = params.visual_client_area.width();
    const int height = tab_strip_type == TabStripType::kWebUi
                           ? views().webui_tab_strip->GetHeightForWidth(width)
                           : 0;
    layout.AddChild(views().webui_tab_strip,
                    gfx::Rect(params.visual_client_area.origin(),
                              gfx::Size(width, height)));
    params.Inset(gfx::Insets::TLBR(height, 0, 0, 0));
  }

  // Lay out horizontal tab strip region if present.
  if (IsParentedTo(views().tab_strip_region_view, views().browser_view)) {
    gfx::Rect tabstrip_bounds;
    if (tab_strip_type == TabStripType::kHorizontal) {
      // Inset the leading edge of the tabstrip by the size of the swoop of the
      // first tab; this is especially important for Mac, where the negative
      // space of the caption button margins and the edge of the tabstrip should
      // overlap. The trailing edge receives the usual treatment, as it is the
      // new tab button and not a tab.
      tabstrip_bounds =
          GetBoundsWithExclusion(params, views().tab_strip_region_view,
                                 TabStyle::Get()->GetBottomCornerRadius());
      SetTop(params, tabstrip_bounds.bottom() -
                         GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP));
      needs_exclusion = false;
    }
    layout.AddChild(views().tab_strip_region_view, tabstrip_bounds,
                    tab_strip_type == TabStripType::kHorizontal);
  }

  // Lay out vertical tab strip if visible.
  if (IsParentedTo(views().vertical_tab_strip_container,
                   views().browser_view)) {
    gfx::Rect vertical_tab_strip_bounds;
    if (tab_strip_type == TabStripType::kVertical) {
      const int vertical_tab_strip_width = std::max(
          kMinVerticalTabStripWidth,
          views().vertical_tab_strip_container->GetPreferredSize().width());
      vertical_tab_strip_bounds = gfx::Rect(
          params.visual_client_area.x(), params.visual_client_area.y(),
          vertical_tab_strip_width, params.visual_client_area.height());
      params.Inset(gfx::Insets::TLBR(0, vertical_tab_strip_width, 0, 0));
    }
    layout.AddChild(views().vertical_tab_strip_container,
                    vertical_tab_strip_bounds,
                    tab_strip_type == TabStripType::kVertical);
  }

  // Figure out whether the toolbar-height side panel should show and by how
  // much.
  const bool has_toolbar_height_side_panel = IsParentedToAndVisible(
      views().toolbar_height_side_panel, views().browser_view);

  // Lay out the main area background.
  if (IsParentedTo(views().main_background_region, views().browser_view)) {
    layout.AddChild(views().main_background_region, params.visual_client_area,
                    has_toolbar_height_side_panel);
  }

  // The insets for main region and its containing views when the
  // toolbar_height_side_panel is visible.
  const int container_inset_padding =
      GetLayoutConstant(LayoutConstant::TOOLBAR_HEIGHT_SIDE_PANEL_INSET);

  // Lay out toolbar-height side panel.
  const double toolbar_height_reveal_amount =
      has_toolbar_height_side_panel
          ? views().toolbar_height_side_panel->GetAnimationValue()
          : 0.0;
  if (IsParentedToAndVisible(views().toolbar_height_side_panel,
                             views().browser_view)) {
    // Side panel needs to fit next to the other stuff in the browser, but it
    // always gets at least its minimum width.
    int target_width =
        views().toolbar_height_side_panel->GetPreferredSize().width();
    target_width = std::min(
        target_width,
        params.visual_client_area.width() -
            (GetMinimumMainAreaSize().width() + container_inset_padding));
    target_width =
        std::max(target_width,
                 views().toolbar_height_side_panel->GetMinimumSize().width());

    // Not all of the width may be visible on the screen.
    const int visible_width =
        base::ClampFloor(target_width * toolbar_height_reveal_amount);

    // Add `container_inset_padding` to the top of the toolbar height side panel
    // to separate it from the tab strip. SidePanel draws the top on top of the
    // top content separator and some units of the toolbar by default, which is
    // not needed for the toolbar height side panel.
    const int top =
        params.visual_client_area.y() +
        std::max(container_inset_padding,
                 base::ClampCeil(
                     params.leading_exclusion.ContentWithPadding().height()));
    gfx::Rect toolbar_height_bounds(
        params.visual_client_area.x() - (target_width - visible_width), top,
        target_width, params.visual_client_area.bottom() - top);
    layout.AddChild(views().toolbar_height_side_panel, toolbar_height_bounds);
    params.Inset(gfx::Insets::TLBR(0, visible_width, 0, 0));
  }

  // As the toolbar height side panel animates in, the main panel shrinks and
  // moves over to accommodate the panel.
  const int scaled_main_area_padding =
      base::ClampRound(toolbar_height_reveal_amount * container_inset_padding);
  params.Inset(gfx::Insets::TLBR(scaled_main_area_padding, 0,
                                 scaled_main_area_padding,
                                 scaled_main_area_padding));

  // Lay out the remainder of the main container.
  layout.AddChild(views().main_shadow_overlay, params.visual_client_area,
                  has_toolbar_height_side_panel);

  // Lay out top container.
  if (IsParentedToAndVisible(views().top_container, views().browser_view)) {
    // Take advantage of the fact that the top container takes up the entire top
    // area of the main area.
    ProposedLayout& top_container_layout =
        layout.AddChild(views().top_container, gfx::Rect());
    // Switch to local coordinates for top container elements.
    const BrowserLayoutParams top_container_params =
        params.InLocalCoordinates(params.visual_client_area);
    top_container_layout.bounds = CalculateTopContainerLayout(
        top_container_layout, top_container_params, needs_exclusion);
    // Convert back to local coordinates.
    top_container_layout.bounds.Offset(
        params.visual_client_area.OffsetFromOrigin());
    SetTop(params, top_container_layout.bounds.bottom());
  }

  // Lay out infobar container.
  if (IsParentedTo(views().infobar_container, views().browser_view)) {
    gfx::Rect infobar_bounds;
    const bool infobar_visible = delegate().IsInfobarVisible();
    if (infobar_visible) {
      infobar_bounds = gfx::Rect(
          params.visual_client_area.x(),
          // Infobar needs to get down out of the way of immersive mode elements
          // in some cases.
          params.visual_client_area.y() + delegate().GetExtraInfobarOffset(),
          params.visual_client_area.width(),
          // This returns zero for empty infobar.
          views().infobar_container->GetPreferredSize().height());
      SetTop(params, infobar_bounds.bottom());
    }
    layout.AddChild(views().infobar_container, infobar_bounds, infobar_visible);
  }

  // Lay out contents-height side panel.
  bool show_left_separator = false;
  bool show_right_separator = false;
  bool side_panel_leading = false;
  int min_contents_width = kContentsContainerMinimumWidth;

  // The contents-height side panel is adjusted for the presence of a top
  // container separator in the browser view.
  const auto* top_separator_layout =
      layout.GetLayoutFor(views().top_container_separator);
  const int side_panel_top =
      top_separator_layout && top_separator_layout->visibility.value()
          ? params.visual_client_area.y() - views::Separator::kThickness
          : params.visual_client_area.y();

  if (IsParentedTo(views().contents_height_side_panel, views().browser_view)) {
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
      int max_width =
          std::min(preferred_width,
                   params.visual_client_area.width() - min_contents_width);
      if (side_panel->ShouldRestrictMaxWidth()) {
        max_width =
            std::min(max_width, params.visual_client_area.width() * 2 / 3);
      }

      // Side panel always gets at least its minimum width.
      side_panel_width = std::max(min_width, max_width);
      side_panel_visible_width = base::ClampFloor(
          side_panel_width *
          views().contents_height_side_panel->GetAnimationValue());
    }

    // Side panel slides in from the edge of the main container..
    const gfx::Rect side_panel_bounds(
        side_panel_leading
            ? params.visual_client_area.x() -
                  (side_panel_width - side_panel_visible_width)
            : params.visual_client_area.right() - side_panel_visible_width,
        side_panel_top, side_panel_width,
        params.visual_client_area.bottom() - side_panel_top);
    layout.AddChild(side_panel, side_panel_bounds);
    InsetHorizontal(params, side_panel_visible_width, side_panel_leading);
  }

  // This will be used to position the separator corner.
  const int separator_edge = side_panel_leading
                                 ? params.visual_client_area.x()
                                 : params.visual_client_area.right();

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
                   views().browser_view)) {
    gfx::Rect separator_bounds;
    if (show_left_separator) {
      const int separator_width =
          views().left_aligned_side_panel_separator->GetPreferredSize().width();
      separator_bounds =
          gfx::Rect(side_panel_leading
                        ? params.visual_client_area.x()
                        : params.visual_client_area.right() - separator_width,
                    params.visual_client_area.y(), separator_width,
                    params.visual_client_area.height());
      InsetHorizontal(params, separator_width, side_panel_leading);
    }
    layout.AddChild(views().left_aligned_side_panel_separator, separator_bounds,
                    show_left_separator);
  }

  // Lay out the right side panel separator.
  if (IsParentedTo(views().right_aligned_side_panel_separator,
                   views().browser_view)) {
    gfx::Rect separator_bounds;
    if (show_right_separator) {
      const int separator_width =
          views()
              .right_aligned_side_panel_separator->GetPreferredSize()
              .width();
      separator_bounds =
          gfx::Rect(side_panel_leading
                        ? params.visual_client_area.x()
                        : params.visual_client_area.right() - separator_width,
                    params.visual_client_area.y(), separator_width,
                    params.visual_client_area.height());
      InsetHorizontal(params, separator_width, side_panel_leading);
    }
    layout.AddChild(views().right_aligned_side_panel_separator,
                    separator_bounds, show_right_separator);
  }

  // Lay out the corner separator.
  if (IsParentedTo(views().side_panel_rounded_corner, views().browser_view)) {
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
  CHECK(
      IsParentedToAndVisible(views().contents_container, views().browser_view));
  CHECK(views().multi_contents_view == nullptr ||
        views().contents_container->Contains(views().multi_contents_view));

  // Because side panels have minimum width, in a small browser, it is possible
  // for the combination of minimum-sized contents pane and minimum-sized side
  // panel may exceed the width of the window. In this case, the contents pane
  // slides under the side panel.
  int content_left = params.visual_client_area.x();
  int content_right = params.visual_client_area.right();
  if (const int deficit =
          min_contents_width - params.visual_client_area.width();
      deficit > 0) {
    // Expand the contents by the deficit on the side with the side panel.
    InsetHorizontal(params, -deficit, side_panel_leading);
    // However, do not let this go past the edge of the allowed area.
    content_left =
        std::max(content_left, browser_params.visual_client_area.x());
    content_right =
        std::min(content_right, browser_params.visual_client_area.right());
  }
  layout.AddChild(views().contents_container,
                  gfx::Rect(content_left, params.visual_client_area.y(),
                            content_right - content_left,
                            params.visual_client_area.height()));

  return layout;
}

gfx::Rect BrowserViewLayoutImpl::CalculateTopContainerLayout(
    ProposedLayout& layout,
    BrowserLayoutParams params,
    bool needs_exclusion) const {
  // Save this so the final bounds can be calculated.
  const int original_top = params.visual_client_area.y();

  const TabStripType tab_strip_type = GetTabStripType();

  // If the WebUI tabstrip is in the top container (which can happen in
  // immersive mode), ensure it is laid out here.
  if (IsParentedTo(views().webui_tab_strip, views().top_container)) {
    const int width = params.visual_client_area.width();
    const int height = tab_strip_type == TabStripType::kWebUi
                           ? views().webui_tab_strip->GetHeightForWidth(width)
                           : 0;
    layout.AddChild(views().webui_tab_strip,
                    gfx::Rect(params.visual_client_area.origin(),
                              gfx::Size(width, height)));
    params.Inset(gfx::Insets::TLBR(height, 0, 0, 0));
  }

  // If the tabstrip is in the top container (which can happen in immersive
  // mode), ensure it is laid out here.
  if (IsParentedTo(views().tab_strip_region_view, views().top_container)) {
    gfx::Rect tabstrip_bounds;
    if (tab_strip_type == TabStripType::kHorizontal) {
      // When there is an exclusion, inset the leading edge of the tabstrip by
      // the size of the swoop of the first tab; this is especially important
      // for Mac, where the negative space of the caption button margins and the
      // edge of the tabstrip should overlap. The trailing edge receives the
      // usual treatment, as it is the new tab button and not a tab.
      tabstrip_bounds =
          GetBoundsWithExclusion(params, views().tab_strip_region_view,
                                 TabStyle::Get()->GetBottomCornerRadius());
      SetTop(params, tabstrip_bounds.bottom() -
                         GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP));
      needs_exclusion = false;
    }
    layout.AddChild(views().tab_strip_region_view, tabstrip_bounds,
                    tab_strip_type == TabStripType::kHorizontal);
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
              : gfx::Rect(params.visual_client_area.x(),
                          params.visual_client_area.y(),
                          params.visual_client_area.width(),
                          views().toolbar->GetPreferredSize().height());
      SetTop(params, toolbar_bounds.bottom());
      needs_exclusion = false;
    }
    layout.AddChild(views().toolbar, toolbar_bounds, toolbar_visible);
  }

  // Lay out the bookmarks bar if one is present.
  const bool bookmarks_visible = delegate().IsBookmarkBarVisible();
  if (IsParentedTo(views().bookmark_bar, views().top_container)) {
    const gfx::Rect bookmarks_bounds(
        params.visual_client_area.x(), params.visual_client_area.y(),
        params.visual_client_area.width(),
        bookmarks_visible ? views().bookmark_bar->GetPreferredSize().height()
                          : 0);
    layout.AddChild(views().bookmark_bar, bookmarks_bounds, bookmarks_visible);
    SetTop(params, bookmarks_bounds.bottom());
  }

  // There are multiple different ways the top separator can render.
  const TopSeparatorType top_separator_type = GetTopSeparatorType();

  // Lay out the loading bar when present.
  if (IsParentedTo(views().loading_bar, views().top_container)) {
    gfx::Rect loading_bar_bounds;
    if (top_separator_type == TopSeparatorType::kLoadingBar) {
      loading_bar_bounds =
          gfx::Rect(params.visual_client_area.x(),
                    params.visual_client_area.y() - kLoadingBarOffset,
                    params.visual_client_area.width(), kLoadingBarHeight);
      SetTop(params, loading_bar_bounds.bottom());
    }
    layout.AddChild(views().loading_bar, loading_bar_bounds,
                    top_separator_type == TopSeparatorType::kLoadingBar);
  }

  // Maybe show the separator in the multi-contents view.
  if (views().multi_contents_view) {
    views().multi_contents_view->SetShouldShowTopSeparator(
        top_separator_type == TopSeparatorType::kMultiContents);
  }

  // Maybe show the separator in the top container.
  if (IsParentedTo(views().top_container_separator, views().top_container)) {
    gfx::Rect separator_bounds;
    if (top_separator_type == TopSeparatorType::kTopContainer) {
      separator_bounds = gfx::Rect(
          params.visual_client_area.x(), params.visual_client_area.y(),
          params.visual_client_area.width(),
          views().top_container_separator->GetPreferredSize().height());
      SetTop(params, separator_bounds.bottom());
    }
    layout.AddChild(views().top_container_separator, separator_bounds,
                    top_separator_type == TopSeparatorType::kTopContainer);
  }

  // In certain circumstances, the top container bounds require adjustment.
  int top = original_top;
  const int height = params.visual_client_area.y() - original_top;

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
  const auto params = delegate().GetBrowserLayoutParams();
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
  const auto params = delegate().GetBrowserLayoutParams();
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
