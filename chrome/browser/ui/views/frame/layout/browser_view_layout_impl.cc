// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/layout/browser_view_layout_impl.h"

#include <map>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_delegate.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/proposed_layout.h"

struct BrowserViewLayoutImpl::ProposedLayout {
  ProposedLayout() = default;
  ~ProposedLayout() = default;

  // Current view's bounds relative to its parent.
  gfx::Rect bounds;

  // Layouts of children of the current view.
  //
  // It is very important that this object not be stored, but only exist on the
  // stack during calls, as the `raw_ptr` may otherwise dangle.
  std::map<raw_ptr<views::View, CtnExperimental>, ProposedLayout> children;

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

  // Applies this layout to `root`. In order to ensure that all child layouts
  // are applied, this is an inherently destructive operation; each child layout
  // is removed as it is applied and if there are any orphan layouts a stack
  // dump is triggered (this will be a CHECK() in the future).
  void ApplyLayout(views::View* root) && {
    for (auto& child : root->children()) {
      if (const auto it = children.find(child); it != children.end()) {
        child->SetBoundsRect(it->second.bounds);
        std::move(it->second).ApplyLayout(child);
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
  CalculateProposedLayout().ApplyLayout(host);
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
  int min_width = kMainBrowserContentsMinimumWidth;
  if (views().multi_contents_view &&
      views().multi_contents_view->IsInSplitView()) {
    // TODO(https://crbug.com/454583671): Eliminate
    // `MultiContentsView::GetMinViewWidth()` in favor of a functional
    // implementation of `GetMinimumSize()`.
    min_width =
        std::max(min_width, 2 * views().multi_contents_view->GetMinViewWidth());
  }
  return min_width;
}

BrowserViewLayoutImpl::ProposedLayout
BrowserViewLayoutImpl::CalculateProposedLayout() const {
  // TODO(https://crbug.com/453717426): Consider caching layouts of the same
  // size if no `InvalidateLayout()` has happened.

  // Build the proposed layout here:
  ProposedLayout layout;
  NOTIMPLEMENTED();
  return layout;
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
  const ProposedLayout layout = CalculateProposedLayout();
  const auto horizontal = GetDialogHorizontalTarget(layout);
  return gfx::Point(horizontal.start() + horizontal.length() / 2,
                    GetDialogTop(layout));
}

gfx::Size BrowserViewLayoutImpl::GetMaximumDialogSize() const {
  const ProposedLayout layout = CalculateProposedLayout();
  const auto horizontal = GetDialogHorizontalTarget(layout);
  const int top = GetDialogTop(layout);
  const int bottom = GetDialogBottom(layout);
  return gfx::Size(horizontal.length(), bottom - top);
}
