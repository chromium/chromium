// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_DRAG_HANDLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_DRAG_HANDLER_H_

#include <memory>
#include <set>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/stack_allocated.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_context.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_controller.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view_targeter_delegate.h"

class TabCollectionNode;
class TabStripModel;
class TabLinkDropHandler;
class TabStripRegionView;
class ExpandOnHoverLock;

enum class DragPositionHint {
  kBefore,  // The drag is before the drag target.
  kAfter    // The drag is after the drag target.
};

// Implements a minimal drag context to interact with the central
// `TabDragController`.
class TabDragHandler : public TabDragContext,
                       public views::ViewTargeterDelegate {
  METADATA_HEADER(TabDragHandler, TabDragContext)
 public:
  explicit TabDragHandler(TabStripModel& tab_strip_model,
                          TabCollectionNode& root_node,
                          TabStripRegionView& tab_strip_region_view);
  ~TabDragHandler() override;
  TabDragHandler(const TabDragHandler&) = delete;
  TabDragHandler& operator=(const TabDragHandler&) = delete;

  // Initializes a drag using `node` as the tab node that received `event`.
  void InitializeDrag(TabCollectionNode& node,
                      const ui::ListSelectionModel& original_selection_model,
                      const ui::LocatedEvent& event);
  // Triggers updates to tab dragging state based on the latest mouse event.
  // Returns a bool indicating whether the drag was successfully handled.
  bool ContinueDrag(views::View& event_source_view,
                    const ui::LocatedEvent& event);
  // Ends the drag, if started.
  void EndDrag(EndDragReason reason);

  // Updates the position of the dragged tabs in the model. Returns true
  // if a model update has been made, false if there are no changes.
  // If `target` is null, the tabs will be inserted into the end of the
  // container.
  bool HandleDraggedTabsIntoPosition(const TabCollectionNode& container_node,
                                     const TabCollectionNode* target);

  // Handles tab strip model updates to reflect dragged tabs entering a node.
  // This reparents them to become direct children of the node.
  void HandleDraggedTabsIntoNode(const TabCollectionNode& node);

  // Returns the drag context for this handler.
  TabDragContext* GetDragContext();

  // Whether this is is handling a drag.
  bool IsDragging() const;

  // Returns true if `view` belongs to a TabCollectionNode currently being
  // dragged.
  bool IsViewDragging(const views::View& view) const;

  // Returns true if there is an ongoing drag that includes a pinned tab.
  bool IsDraggingPinnedTabs() const;

  // Returns true if there is an ongoing drag where a group is being moved.
  bool IsDraggingGroups() const;

  // For some tabs (e.g. split tabs or tabs in a dragged group), the attached
  // view in `TabDragData` doesn't represent the actual container view being
  // dragged. This method converts `view` to the actual view being dragged.
  views::View* GetDraggedViewForAttachedView(views::View* view) const;

  // Returns the starting position of the view when dragging started.
  // The position is in screen coordinates.
  std::optional<gfx::Vector2d> GetOffsetFromSourceAtDragStart(
      views::View* view) const;

  // Returns the DropIndex for a given node and position hint.
  // For tab nodes, a nullopt position hint indicates that the drop is over the
  // middle of the tab and should be interpreted as a "replace" operation.
  std::optional<BrowserRootView::DropIndex> GetLinkDropIndexForNode(
      const TabCollectionNode& node,
      std::optional<DragPositionHint> position_hint) const;

  // Called before a tab is added to the tabstrip. Cancels the current drag if
  // there is one.
  void OnTabWillBeAdded();

  // Called before a tab is removed from the tabstrip. Cancels the current drag
  // if the tab is one of the dragged tabs.
  void OnTabWillBeRemoved(content::WebContents* contents);

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  // TabDragContext
  bool CanAcceptEvent(const ui::Event& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  TabDragContext* GetContextForNewBrowser(
      BrowserView* browser_view) const override;
  views::View* GetTabForContents(content::WebContents* contents) override;
  content::WebContents* GetContentsForTab(views::View* tab) override;
  bool IsTabDetachable(const views::View* view) const override;
  void SetTabDetached(views::View* view) override;
  views::View* GetTabGroupHeader(
      const tab_groups::TabGroupId& group_id) override;
  std::optional<tab_groups::TabGroupId> GetTabGroupForHeader(
      const views::View* header_view) const override;
  TabStripModel* GetTabStripModel() override;
  TabDragController* GetDragController() override;
  void OwnDragController(
      std::unique_ptr<TabDragController> controller) override;
  std::unique_ptr<TabDragController> ReleaseDragController() override;
  void DestroyDragController() override;
  void StartedDragging(const std::vector<views::View*>& views) override;
  void DraggedTabsDetached() override;
  void StoppedDragging() override;
  void SetDragControllerCallbackForTesting(
      base::OnceCallback<void(TabDragController*)> callback) override;
  TabDragPositioningDelegate* GetPositioningDelegate() override;
  bool NotifyCustomEvent(ui::CustomElementEventType event_type,
                         views::View* view) override;

  // The following overrides are necessary to support SystemDnD.
  bool CanDrop(const OSExchangeData& data) override;
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;

 private:
  // Encapsulates data needed to initialize a drag session.
  struct DragInitData {
    STACK_ALLOCATED();

   public:
    DragInitData();
    ~DragInitData();
    DragInitData(const DragInitData&);
    DragInitData& operator=(const DragInitData&);

    raw_ptr<views::View> source_dragged_view = nullptr;
    std::vector<views::View*> dragged_views;
    ui::ListSelectionModel list_selection_model;
  };

  // Helpers for initialize a drag, according to the type of `source_node`.
  DragInitData GetDragInitDataForTabDrag(TabCollectionNode& source_node);
  DragInitData GetDragInitDataForGroupHeaderDrag(
      TabCollectionNode& source_node);
  std::map<tab_groups::TabGroupId, views::View*> GetFullySelectedGroups(
      const std::vector<tabs::TabInterface*>& selected_tabs);

  TabCollectionNode* GetNodeForContents(content::WebContents* contents);
  TabCollectionNode* GetNodeForTabGroup(const tab_groups::TabGroupId& group_id);
  const TabCollectionNode* GetNodeForTabGroup(
      const tab_groups::TabGroupId& group_id) const;

  // Resets member variables that track a drag being managed by this handler.
  void ResetDragState();

  // Returns the group id of the dragged group header, or null if the drag
  // was not initiated by a group header.
  std::optional<tab_groups::TabGroupId> GetDraggingGroupHeaderId() const;

  const raw_ref<TabStripModel> tab_strip_model_;
  const raw_ref<TabCollectionNode> root_node_;
  const raw_ref<TabStripRegionView> tab_strip_region_view_;

  std::unique_ptr<TabLinkDropHandler> link_drop_handler_;

  // Null if this handler is not managing a dragging session.
  std::unique_ptr<TabDragController> drag_controller_ = nullptr;

  std::unique_ptr<ExpandOnHoverLock> expand_on_hover_lock_;

  std::vector<std::unique_ptr<views::ScrollView::ScopedScrollSynchronizer>>
      scroll_synchronizers_;

  base::WeakPtrFactory<TabDragHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_DRAG_HANDLER_H_
