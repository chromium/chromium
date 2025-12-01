// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_IMPL_H_

#include "chrome/browser/ui/views/frame/layout/browser_view_layout.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_params.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view.h"

// Represents common functionality between browser layouts when one or more of
// the following flags are enabled:
//  - `features::kAppBrowserUseNewLayout`
//  - `features::kPopupBrowserUseNewLayout`
//  - `features::kTabbedBrowserUseNewLayout`
//
// Contains a number of common layout and utility methods, as well as constants
// and structures used across these layouts.
//
// `BrowserViewLayoutImplOld` is still used for legacy apps, popups, and a
// handful of other browser configurations.
class BrowserViewLayoutImpl : public BrowserViewLayout {
 public:
  BrowserViewLayoutImpl(std::unique_ptr<BrowserViewLayoutDelegate> delegate,
                        Browser* browser,
                        BrowserViewLayoutViews views);
  ~BrowserViewLayoutImpl() override;

  // BrowserViewLayout:
  void Layout(views::View* host) override;

  // BrowserViewLayout overrides:
  int GetMinWebContentsWidthForTesting() const override;

 protected:
  // The minimum width of the contents area itself. Applies even when side
  // panels are open and prevents zero or negative contents sizes.
  static constexpr int kContentsContainerMinimumWidth = 200;

  // The overlap between a constrained dialog and the toolbar.
  static constexpr int kDialogToolbarOverlap = 3;

  // Describes a proposed layout in BrowserView. Unlike the equivalent
  // `LayoutManagerBase` concept, this struct is hierarchical, representing an
  // entire Views subtree.
  struct ProposedLayout {
    ProposedLayout(const gfx::Rect& bounds_, std::optional<bool> visibility_);
    ProposedLayout();
    ProposedLayout(ProposedLayout&&) noexcept;
    ProposedLayout& operator=(ProposedLayout&&) noexcept;
    ~ProposedLayout();

    // Current view's bounds relative to its parent.
    gfx::Rect bounds;

    // If visibility is to be set during layout, set this flag.
    std::optional<bool> visibility;

    // Layouts of children of the current view.
    //
    // It is very important that this object not be stored, but only exist on
    // the stack during calls, as the `raw_ptr` may otherwise dangle.
    std::map<raw_ptr<views::View, CtnExperimental>, ProposedLayout> children;

    // Adds a child layout for `child` and returns the layout. Fails if `child`
    // is already present.
    ProposedLayout& AddChild(views::View* child,
                             const gfx::Rect& bounds_,
                             std::optional<bool> visibility_ = std::nullopt);

    // Searches the tree for `descendant` and returns its layout, otherwise,
    // returns null if not found.
    const ProposedLayout* GetLayoutFor(const views::View* descendant) const;

    // Finds `descendant`'s layout in the tree and returns its bounds relative
    // to `relative_to`.
    std::optional<gfx::Rect> GetBoundsFor(const views::View* descendant,
                                          const views::View* relative_to) const;

    using SetViewVisibility =
        base::FunctionRef<void(views::View* view, bool visible)>;

    // Applies this layout to `root`. In order to ensure that all child layouts
    // are applied, this is an inherently destructive operation; each child
    // layout is removed as it is applied and if there are any orphan layouts a
    // stack dump is triggered (this will be a CHECK() in the future).
    void ApplyLayout(views::View* root,
                     SetViewVisibility set_view_visibility) &&;
  };

  // Shorthand for validating both `child` and `parent` and checking that one is
  // parented to the other. Ignores child visibility.
  static bool IsParentedTo(const views::View* child, const views::View* parent);

  // Shorthand for validating both `child` and `parent` and checking that one is
  // parented to the other. If `child` is not visible, returns false.
  static bool IsParentedToAndVisible(const views::View* child,
                                     const views::View* parent);

  // Gets the bounds for a `view`, placed between the exclusion zones in
  // `params` if they are present.
  static gfx::Rect GetBoundsWithExclusion(const BrowserLayoutParams& params,
                                          const views::View* view,
                                          int leading_margin = 0,
                                          int trailing_margin = 0);

  // Converts `local_bounds` to coordinates of `parent_params` after applying
  // common adjustments for e.g. immersive mode.
  gfx::Rect GetTopContainerBoundsInParent(
      const gfx::Rect& local_bounds,
      const BrowserLayoutParams& parent_params) const;

  // Override these methods to customize layout for specific browser types.

  // Hierarchical version of views::ProposedLayout that will allow us to run
  // calculations without actually applying the layout.
  //
  // Calculates the layout of all elements parented to a `BrowserView`, but not
  // elements on a separate overlay widget (e.g. on Mac in immersive mode).
  virtual ProposedLayout CalculateProposedLayout(
      const BrowserLayoutParams& params) const = 0;

  // Lays out the elements of the top container, updating both `layout` and
  // `params`. Used when laying out the top container in both the `BrowserView`
  // an in overlay widgets in immersive mode.
  //
  // The children of the container will be added to `layout`. Returns the local
  // bounds of the top container, which must then be translated to the parent
  // before being stored (see `GetTopContainerBoundsInParent()`).
  virtual gfx::Rect CalculateTopContainerLayout(ProposedLayout& layout,
                                                BrowserLayoutParams params,
                                                bool needs_exclusion) const = 0;

  // Applies additional visual adjustments to UI elements that are not handled
  // by the traditional layout process. This could include clipping, text
  // rendering, overlay configuration, etc.
  virtual void DoPostLayoutVisualAdjustments() {}

 private:
  // Retrieve dimensions of modal dialogs.

  // Gets the top of the dialog anchoring area, in local coordinates.
  int GetDialogTop(const ProposedLayout& layout) const;

  // Gets the bottom of the dialog anchoring area, in local coordinates.
  int GetDialogBottom(const ProposedLayout& layout) const;

  // BrowserViewLayout overrides:
  gfx::Point GetDialogPosition(const gfx::Size& dialog_size) const override;
  gfx::Size GetMaximumDialogSize() const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_IMPL_H_
