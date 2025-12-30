// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_DRAG_HANDLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_DRAG_HANDLER_H_

#include <memory>
#include <set>
#include <vector>

#include "base/callback_list.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_context.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_types.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"

class TabCollectionNode;
class TabStripModel;

// Interface for views to interact with drag handling.
class VerticalTabDragHandler {
 public:
  virtual ~VerticalTabDragHandler() = default;
  // Initializes a drag using `node` as the tab node that received `event`.
  virtual void InitializeDrag(TabCollectionNode& node,
                              const ui::MouseEvent& event) = 0;
  // Triggers updates to tab dragging state based on the latest mouse event.
  // Returns a bool indicating whether the drag was successfully handled.
  virtual bool ContinueDrag(views::View& event_source_view,
                            const ui::MouseEvent& event) = 0;
  // Ends the drag, if started.
  virtual void EndDrag(EndDragReason reason) = 0;

  // Handles tab strip model updates to reflect a drag over a give tab node.
  virtual void DraggedTabsOverNode(const TabCollectionNode& node) = 0;

  // Returns the drag context for this handler.
  virtual TabDragContext* GetDragContext() = 0;

  // For vertical tabs, `TabSlotView` doesn't represent the actual tab
  // view. This method converts `view` to its actual tab view, or nullptr
  // if this handler doesn't manage it.
  static views::View* ViewFromTabSlot(TabSlotView* view);
};

// Implements a minimal drag context to interact with the central
// `TabDragController`.
// TODO(crbug.com/439963720): The following is an incremental checklist of
// support that needs to be added:
// - Dragging more than one tab (split tabs, tab group, multi-selection).
// - Dragging pinned tab (split tabs, tab group, multi-selection).
class VerticalTabDragHandlerImpl : public VerticalTabDragHandler,
                                   public TabDragContext {
  METADATA_HEADER(VerticalTabDragHandlerImpl, TabDragContext)
 public:
  VerticalTabDragHandlerImpl(TabStripModel& tab_strip_model,
                             TabCollectionNode& root_node);
  ~VerticalTabDragHandlerImpl() override;
  VerticalTabDragHandlerImpl(const VerticalTabDragHandlerImpl&) = delete;
  VerticalTabDragHandlerImpl& operator=(const VerticalTabDragHandlerImpl&) =
      delete;

  // VerticalTabDragHandler
  void InitializeDrag(TabCollectionNode& node,
                      const ui::MouseEvent& event) override;
  bool ContinueDrag(views::View& event_source_view,
                    const ui::MouseEvent& event) override;
  void EndDrag(EndDragReason reason) override;
  void DraggedTabsOverNode(const TabCollectionNode& node) override;
  TabDragContext* GetDragContext() override;

  // TabDragContext
  bool CanAcceptEvent(const ui::Event& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  TabDragContext* GetContextForNewBrowser(
      BrowserView* browser_view) const override;
  TabSlotView* GetTabForContents(content::WebContents* contents) override;
  content::WebContents* GetContentsForTab(TabSlotView* tab) override;
  bool IsTabDetachable(const TabSlotView* view) const override;
  bool IsTabPinned(const TabSlotView* tab) const override;
  int GetTabCount() const override;
  int GetPinnedTabCount() const override;
  TabGroupHeader* GetTabGroupHeader(
      const tab_groups::TabGroupId& group) const override;
  TabStripModel* GetTabStripModel() override;
  TabDragController* GetDragController() override;
  void OwnDragController(
      std::unique_ptr<TabDragController> controller) override;
  std::unique_ptr<TabDragController> ReleaseDragController() override;
  void DestroyDragController() override;
  void StartedDragging(const std::vector<TabSlotView*>& views) override;
  void DraggedTabsDetached() override;
  void StoppedDragging() override;
  void SetDragControllerCallbackForTesting(
      base::OnceCallback<void(TabDragController*)> callback) override;
  TabDragPositioningDelegate* GetPositioningDelegate() override;

 private:
  TabCollectionNode* GetNodeForContents(content::WebContents* contents);

  // Creates a `TabSlotView` shim for `node`, used for compatibility with the
  // core dragging system. The created shim view is cached in `shim_views_`.
  TabSlotView& GetOrCreateShimViewForNode(TabCollectionNode& node);

  // Resets member variables that track a drag being managed by this handler.
  void ResetDragState();

  // Cleans up state tracked by this handler for a given node.
  void OnNodeWillDestroy(TabCollectionNode& node);

  const raw_ref<TabStripModel> tab_strip_model_;
  const raw_ref<TabCollectionNode> root_node_;

  // Null if this handler is not managing a dragging session.
  std::unique_ptr<TabDragController> drag_controller_ = nullptr;

  // The tabs currently being dragged as part of a dragging session managed by
  // this handler.
  std::set<raw_ptr<const TabCollectionNode>> dragged_tabs_;

  // A mapping from nodes to their `TabSlotView` shims, used for compatibility
  // with the core dragging system.
  std::map<raw_ptr<const TabCollectionNode>, raw_ptr<TabSlotView>> shim_views_;
  std::vector<base::CallbackListSubscription> node_destroyed_callbacks_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_DRAG_HANDLER_H_
