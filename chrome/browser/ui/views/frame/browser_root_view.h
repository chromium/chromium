// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_ROOT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_ROOT_VIEW_H_

#include <memory>

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
 public:
  METADATA_HEADER(BrowserRootView);

  struct DropIndex {
    // The index within the tabstrip to drop on/before (see
    // |insert_before_index| below).
    int value = 0;

    // If true, the dropped item should be inserted before |tab_index|.
    // If false, the dropped item should replace the tab at |tab_index|.
    bool drop_before = false;

    // If |drop_before| is true, and |value| is the first tab in a tab
    // group, determines whether to drop in the group or just before it.
    // This disambiguates a drop before or after a group header.
    bool drop_in_group = false;

    bool operator==(const DropIndex& other) const {
      return value == other.value && drop_before == other.drop_before &&
             drop_in_group == other.drop_in_group;
    }
  };

  class DropTarget {
   public:
    DropTarget(const DropTarget&) = delete;
    DropTarget& operator=(const DropTarget&) = delete;

    virtual DropIndex GetDropIndex(const ui::DropTargetEvent& event) = 0;
    virtual DropTarget* GetDropTarget(gfx::Point loc_in_local_coords) = 0;
    virtual views::View* GetViewForDrop() = 0;

    virtual void HandleDragUpdate(const absl::optional<DropIndex>& index) {}
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
  gfx::Size CalculatePreferredSize() const override;

 protected:
  // views::View:
  void PaintChildren(const views::PaintInfo& paint_info) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserRootViewBrowserTest, ClearDropInfo);

  // Used during a drop session of a url. Tracks the position of the drop.
  struct DropInfo {
    DropInfo();
    ~DropInfo();

    raw_ptr<DropTarget, DanglingUntriaged> target = nullptr;

    // Where to drop the url.
    absl::optional<DropIndex> index;

    // The URL for the drop event.
    GURL url;

    // Whether the MIME type of the file pointed to by |url| is supported.
    // TODO(sangwoo108) Try removing this memeber.
    bool file_supported = true;
  };

  // Converts the event from the hosts coordinate system to the view's
  // coordinate system.
  DropIndex GetDropIndexForEvent(const ui::DropTargetEvent& event,
                                 const ui::OSExchangeData& data,
                                 DropTarget* target);

  DropTarget* GetDropTarget(const ui::DropTargetEvent& event);

  // Called to indicate whether the given URL is a supported file.
  void OnFileSupported(const GURL& url, bool supported);

  TabStrip* tabstrip() { return browser_view_->tabstrip(); }
  ToolbarView* toolbar() { return browser_view_->toolbar(); }

  // Returns true if |data| has string contents and the user can "paste and go".
  // If |url| is non-null and the user can "paste and go", |url| is set to the
  // desired destination.
  bool GetPasteAndGoURL(const ui::OSExchangeData& data, GURL* url);

  // Navigates to the dropped URL.
  void NavigateToDropUrl(
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

  base::WeakPtrFactory<BrowserRootView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_ROOT_VIEW_H_
