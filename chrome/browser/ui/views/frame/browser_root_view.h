// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_ROOT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_ROOT_VIEW_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/widget/root_view.h"

class ToolbarView;

namespace ui {
class OSExchangeData;
}

// RootView implementation used by BrowserFrame. This forwards drop events to
// the TabStrip. Visually the tabstrip extends to the top of the frame, but in
// actually it doesn't. The tabstrip is only as high as a tab. To enable
// dropping above the tabstrip BrowserRootView forwards drop events to the
// TabStrip.
class BrowserRootView : public views::internal::RootView {
  METADATA_HEADER(BrowserRootView, views::internal::RootView)

 public:
  struct DropIndex {
    // The index within the tabstrip to drop on/before (see `relative_to_index`
    // below).
    int index = 0;

    // Whether the dropped item should be inserted before `index` or replace
    // the tab at `index`.
    enum class RelativeToIndex { kInsertBeforeIndex, kReplaceIndex };
    RelativeToIndex relative_to_index = RelativeToIndex::kReplaceIndex;

    // If `relative_to_index` is `kInsertBeforeIndex`, and `index` is the first
    // tab in a tab group, determines whether to drop in the group or just
    // before it. This disambiguates a drop before or after a group header.
    enum class GroupInclusion { kIncludeInGroup, kDontIncludeInGroup };
    GroupInclusion group_inclusion = GroupInclusion::kDontIncludeInGroup;

    bool operator==(const DropIndex& other) const = default;
  };

  class DropTarget {
   public:
    DropTarget(const DropTarget&) = delete;
    DropTarget& operator=(const DropTarget&) = delete;

    // Returns a `DropIndex` for the drop. Returns `nullopt` if it is not
    // possible to drop at this location.
    virtual std::optional<DropIndex> GetDropIndex(
        const ui::DropTargetEvent& event) = 0;
    virtual DropTarget* GetDropTarget(gfx::Point loc_in_local_coords) = 0;
    virtual views::View* GetViewForDrop() = 0;

    virtual void HandleDragUpdate(const std::optional<DropIndex>& index) {}
    virtual void HandleDragExited() {}

   protected:
    DropTarget() = default;
    virtual ~DropTarget() = default;
  };

  // You must call set_tabstrip before this class will accept drops.
  BrowserRootView(BrowserView* browser_view, views::Widget* widget);
  BrowserRootView(const BrowserRootView&) = delete;
  BrowserRootView& operator=(const BrowserRootView&) = delete;
  ~BrowserRootView() override;

  // views::View:
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool AreDropTypesRequired() override;
  bool CanDrop(const ui::OSExchangeData& data) override;
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  DropCallback GetDropCallback(const ui::DropTargetEvent& event) override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

 protected:
  // views::View:
  void PaintChildren(const views::PaintInfo& paint_info) override;

 private:
  friend class BrowserRootViewBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(BrowserRootViewBrowserTest, ClearDropInfo);
  FRIEND_TEST_ALL_PREFIXES(BrowserRootViewBrowserTest, DropOrderingCorrect);

  // Used during a drop session of a url. Tracks the position of the drop.
  struct DropInfo {
    DropInfo();
    ~DropInfo();

    raw_ptr<DropTarget, DanglingUntriaged> target = nullptr;

    // Where to drop the urls.
    std::optional<DropIndex> index;

    // The validated URLs for the drop event.
    std::vector<GURL> urls;

    // An incrementing sequence number for `DropInfo`s.
    int sequence = 0;

    // Set to true when the filtering of the URLs being dropped is complete.
    bool filtering_complete = false;
  };

  // Converts `event` from the hosts coordinate system to the view's
  // coordinate system, and gets the `DropIndex` for the drop.
  std::optional<DropIndex> GetDropIndexForEvent(
      const ui::DropTargetEvent& event,
      const ui::OSExchangeData& data,
      DropTarget* target);

  DropTarget* GetDropTarget(const ui::DropTargetEvent& event);

  // Called when the filtering for supported URLs is complete.
  void OnFilteringComplete(int sequence, std::vector<GURL> urls);

  // Sets a callback for when URL filtering is complete. Be sure to wait for
  // filtering to be complete before checking the drag operation returned by
  // `OnDragUpdated()` or calling the drop callback in tests.
  void SetOnFilteringCompleteClosureForTesting(base::OnceClosure closure);

  TabStrip* tabstrip() { return browser_view_->tabstrip(); }
  ToolbarView* toolbar() { return browser_view_->toolbar(); }

  // Returns a URL if |data| has string contents and the user can "paste and
  // go".
  std::optional<GURL> GetPasteAndGoURL(const ui::OSExchangeData& data);

  // Navigates to the dropped URLs.
  void NavigateToDroppedUrls(
      std::unique_ptr<DropInfo> drop_info,
      const ui::DropTargetEvent& event,
      ui::mojom::DragOperation& output_drag_op,
      std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner);

  // The BrowserView.
  raw_ptr<BrowserView, AcrossTasksDanglingUntriaged> browser_view_ = nullptr;

  // Used to calculate partial offsets in scrolls that occur for a smooth
  // scroll device.
  int scroll_remainder_x_ = 0;
  int scroll_remainder_y_ = 0;

  std::unique_ptr<DropInfo> drop_info_;

  base::OnceClosure on_filtering_complete_closure_;

  base::WeakPtrFactory<BrowserRootView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_ROOT_VIEW_H_
