// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_DRAG_HANDLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_DRAG_HANDLER_H_

#include <memory>
#include <set>
#include <vector>

#include "base/callback_list.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_context.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_types.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/vector2d.h"

class TabCollectionNode;
class TabStripModel;
class VerticalTabLinkDropHandler;

enum class DragPositionHint {
  kBefore,  // The drag is before the drag target.
  kAfter    // The drag is after the drag target.
};

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

  // Handles tab strip model updates to reflect a drag over a given node.
  // Position hint is used to determine where the drag is, relative to the node.
  virtual void HandleDraggedTabsOverNode(
      const TabCollectionNode& node,
      std::optional<DragPositionHint> position_hint) = 0;

  // Handles tab strip model updates to reflect dragged tabs entering a node.
  // This reparents them to become direct children of the node.
  virtual void HandleDraggedTabsIntoNode(const TabCollectionNode& node) = 0;

  // Handles tab strip model updates to reflect a drag exiting a group.
  // Position hint is used to determine where the drag is, relative to the node.
  virtual void HandleDraggedTabsOutOfGroup(const TabCollectionNode& node,
                                           DragPositionHint position_hint) = 0;

  // Handles the case where tabs are dragged to the end of the tab strip, which
  // is a special case because there is no node there to handle the drag.
  virtual void HandleDraggedTabsAtEndOfTabStrip() = 0;

  // Returns the drag context for this handler.
  virtual TabDragContext* GetDragContext() = 0;

  // Whether this is is handling a drag.
  virtual bool IsDragging() const = 0;

  // Returns true if `view` belongs to a TabCollectionNode currently being
  // dragged.
  virtual bool IsViewDragging(const views::View& view) const = 0;

  // Returns true if there is an ongoing drag that includes a pinned tab.
  virtual bool IsDraggingPinnedTabs() const = 0;

  // Returns true if there is an ongoing drag where a group is being moved.
  virtual bool IsDraggingGroups() const = 0;

  // Returns true if the drag is currently at the end of the tab strip.
  virtual bool IsDraggingAtEndOfTabStrip() const = 0;

  // For vertical tabs, `TabSlotView` doesn't represent the actual tab
  // view. This method converts `view` to its actual tab view, or nullptr
  // if this handler doesn't manage it.
  virtual views::View* ViewFromTabSlot(TabSlotView* view) const = 0;

  // Returns the starting position of the view when dragging started.
  // The position is in screen coordinates.
  virtual std::optional<gfx::Vector2d> GetOffsetFromSourceAtDragStart(
      views::View* view) const = 0;

  // Returns the DropIndex for a given node and position hint.
  // For tab nodes, a nullopt position hint indicates that the drop is over the
  // middle of the tab and should be interpreted as a "replace" operation.
  virtual std::optional<BrowserRootView::DropIndex> GetLinkDropIndexForNode(
      const TabCollectionNode& node,
      std::optional<DragPositionHint> position_hint) const = 0;
};

// Implements a minimal drag context to interact with the central
// `TabDragController`.
class VerticalTabDragHandlerImpl : public VerticalTabDragHandler,
                                   public TabDragContext {
  METADATA_HEADER(VerticalTabDragHandlerImpl, TabDragContext)
 public:
  explicit VerticalTabDragHandlerImpl(TabStripModel& tab_strip_model,
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
  void HandleDraggedTabsOverNode(
      const TabCollectionNode& node,
      std::optional<DragPositionHint> position_hint) override;
  void HandleDraggedTabsIntoNode(const TabCollectionNode& node) override;
  void HandleDraggedTabsOutOfGroup(const TabCollectionNode& node,
                                   DragPositionHint position_hint) override;
  void HandleDraggedTabsAtEndOfTabStrip() override;
  TabDragContext* GetDragContext() override;
  bool IsDragging() const override;
  bool IsViewDragging(const views::View& view) const override;
  bool IsDraggingPinnedTabs() const override;
  bool IsDraggingGroups() const override;
  bool IsDraggingAtEndOfTabStrip() const override;
  views::View* ViewFromTabSlot(TabSlotView* view) const override;
  std::optional<gfx::Vector2d> GetOffsetFromSourceAtDragStart(
      views::View* view) const override;
  std::optional<BrowserRootView::DropIndex> GetLinkDropIndexForNode(
      const TabCollectionNode& node,
      std::optional<DragPositionHint> position_hint) const override;

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
  TabSlotView* GetTabGroupHeader(
      const tab_groups::TabGroupId& group_id) override;
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
    DragInitData();
    ~DragInitData();
    DragInitData(const DragInitData&);
    DragInitData& operator=(const DragInitData&);

    raw_ptr<TabSlotView> source_dragged_view = nullptr;
    std::vector<TabSlotView*> dragged_views;
    ui::ListSelectionModel list_selection_model;
  };

  // Helpers for initialize a drag, according to the type of `source_node`.
  // These helpers will create the `TabSlotView`s for the dragged
  // tabs as needed.
  DragInitData GetDragInitDataForTabDrag(TabCollectionNode& source_node);
  DragInitData GetDragInitDataForGroupHeaderDrag(
      TabCollectionNode& source_node);
  std::map<tab_groups::TabGroupId, TabSlotView*> GetFullySelectedGroups(
      const std::vector<tabs::TabInterface*>& selected_tabs);

  TabCollectionNode* GetNodeForContents(content::WebContents* contents);
  TabCollectionNode* GetNodeForTabGroup(const tab_groups::TabGroupId& group_id);
  const TabCollectionNode* GetNodeForTabGroup(
      const tab_groups::TabGroupId& group_id) const;

  // Creates a `TabSlotView` for `node`, used for compatibility with the
  // core dragging system. The created slot view is cached in `slot_views_`.
  TabSlotView& GetOrCreateSlotViewForNode(TabCollectionNode& node);

  // Resets member variables that track a drag being managed by this handler.
  void ResetDragState();

  // Cleans up state tracked by this handler for a given node.
  void OnNodeWillDestroy(TabCollectionNode& node);

  // Handlers for drag operations over various node types.
  void HandleTabDragOverTab(const TabCollectionNode& node);
  void HandleTabDragOverSplit(const TabCollectionNode& node);
  void HandleTabDragOverGroup(const TabCollectionNode& node);

  // Returns the group id of the dragged group header, or null if the drag
  // was not initiated by a group header.
  std::optional<tab_groups::TabGroupId> GetDraggingGroupHeaderId() const;

  const raw_ref<TabStripModel> tab_strip_model_;
  const raw_ref<TabCollectionNode> root_node_;

  std::unique_ptr<VerticalTabLinkDropHandler> link_drop_handler_;

  // Null if this handler is not managing a dragging session.
  std::unique_ptr<TabDragController> drag_controller_ = nullptr;

  // A mapping from nodes to their `TabSlotView`, used for compatibility
  // with the core dragging system.
  std::map<raw_ptr<const TabCollectionNode>, raw_ptr<TabSlotView>> slot_views_;
  std::vector<base::CallbackListSubscription> node_destroyed_callbacks_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_DRAG_HANDLER_H_
