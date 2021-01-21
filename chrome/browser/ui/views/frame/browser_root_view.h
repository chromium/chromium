// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_ROOT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_ROOT_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
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
  struct DropIndex {
    // The index within the tabstrip to drop on/before (see
    // |insert_before_index| below).
    int value = 0;

    // If true, the dropped item should be inserted before |tab_index|.
    // If false, the dropped item should replace the tab at |tab_index|.
    bool drop_before = false;

    bool operator==(const DropIndex& other) const {
      return value == other.value && drop_before == other.drop_before;
    }
  };

  class DropTarget {
   public:
    virtual DropIndex GetDropIndex(const ui::DropTargetEvent& event) = 0;
    virtual views::View* GetViewForDrop() = 0;

    virtual void HandleDragUpdate(const base::Optional<DropIndex>& index) {}
    virtual void HandleDragExited() {}

   protected:
    DropTarget() = default;
    virtual ~DropTarget() = default;

   private:
    DISALLOW_COPY_AND_ASSIGN(DropTarget);
  };

  // Internal class name.
  static const char kViewClassName[];

  // You must call set_tabstrip before this class will accept drops.
  BrowserRootView(BrowserView* browser_view, views::Widget* widget);
  ~BrowserRootView() override;

  // views::View:
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool AreDropTypesRequired() override;
  bool CanDrop(const ui::OSExchangeData& data) override;
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  int OnPerformDrop(const ui::DropTargetEvent& event) override;
  const char* GetClassName() const override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

 protected:
  // views::View:
  void PaintChildren(const views::PaintInfo& paint_info) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserRootViewBrowserTest, ClearDropInfo);

  // Used during a drop session of a url. Tracks the position of the drop.
  struct DropInfo {
    DropInfo();
    ~DropInfo();

    DropTarget* target = nullptr;

    // Where to drop the url.
    base::Optional<DropIndex> index;

    // The URL for the drop event.
    GURL url;

    // Whether the MIME type of the file pointed to by |url| is supported.
    // TODO(sangwoo108) Try removing this memeber.
    bool file_supported = true;
  };

  // ui::EventProcessor:
  void OnEventProcessingStarted(ui::Event* event) override;

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

  // The BrowserView.
  BrowserView* browser_view_ = nullptr;

  // Used to calculate partial offsets in scrolls that occur for a smooth
  // scroll device.
  int scroll_remainder_x_ = 0;
  int scroll_remainder_y_ = 0;

  std::unique_ptr<DropInfo> drop_info_;

  base::WeakPtrFactory<BrowserRootView> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BrowserRootView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_ROOT_VIEW_H_
