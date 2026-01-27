// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_drag_handler.h"

#include <algorithm>
#include <memory>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/types/to_address.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_controller.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip_types.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_controller.h"
#include "components/tabs/public/split_tab_collection.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/compositor/layer.h"
#include "ui/views/view_utils.h"

namespace {

// This is a shim around `TabCollectionNode` to make it compatible with the
// tab dragging logic in `TabDragController`. Longer term, core tab dragging
// logic will be updated to remove the `TabSlotView` dependency, which
// should make this shim view no longer needed.
class VerticalTabSlotView : public TabSlotView {
  METADATA_HEADER(VerticalTabSlotView, TabSlotView)
 public:
  explicit VerticalTabSlotView(const TabCollectionNode& node) : node_(node) {
    // TODO(crbug.com/439963720): Support dragging other types.
    CHECK(node_->type() == TabCollectionNode::Type::TAB);
  }

  ~VerticalTabSlotView() override = default;
  VerticalTabSlotView(const VerticalTabSlotView&) = delete;
  VerticalTabSlotView& operator=(const VerticalTabSlotView&) = delete;

  TabSlotView::ViewType GetTabSlotViewType() const override {
    return ViewType::kTab;
  }

  TabSizeInfo GetTabSizeInfo() const override { return TabSizeInfo(); }

  const TabCollectionNode& node() const { return node_.get(); }

 private:
  const raw_ref<const TabCollectionNode> node_;
};

BEGIN_METADATA(VerticalTabSlotView)
END_METADATA

ui::mojom::DragEventSource EventSourceFromEvent(const ui::LocatedEvent& event) {
  return event.IsGestureEvent() ? ui::mojom::DragEventSource::kTouch
                                : ui::mojom::DragEventSource::kMouse;
}

}  // namespace

VerticalTabDragHandlerImpl::VerticalTabDragHandlerImpl(
    TabStripModel& tab_strip_model,
    TabCollectionNode& root_node)
    : tab_strip_model_(tab_strip_model), root_node_(root_node) {}

VerticalTabDragHandlerImpl::~VerticalTabDragHandlerImpl() = default;

void VerticalTabDragHandlerImpl::InitializeDrag(TabCollectionNode& node,
                                                const ui::MouseEvent& event) {
  // TODO(crbug.com/439963720): Look into why the state is not reset elsewhere
  // after initializing a drag.
  ResetDragState();
  drag_controller_ = std::make_unique<TabDragController>();

  const auto& selected_tabs =
      tab_strip_model_->selection_model().selected_tabs();

  std::vector<TabSlotView*> dragged_views;
  dragged_views.reserve(selected_tabs.size());

  TabSlotView* source_dragged_view = nullptr;

  CHECK_EQ(node.type(), TabCollectionNode::Type::TAB);
  const auto* source_tab =
      std::get<const tabs::TabInterface*>(node.GetNodeData());

  // Track the node and build a shim view for each selected node.
  ui::ListSelectionModel list_selection_model;
  for (tabs::TabInterface* tab : selected_tabs) {
    // Filter out selections that don't match the pinned state of the latest
    // selected tab.
    if (source_tab->IsPinned() != tab->IsPinned()) {
      continue;
    }
    size_t index = tab_strip_model_->GetIndexOfTab(tab);
    list_selection_model.AddIndexToSelection(index);
    TabCollectionNode* selected_node =
        root_node_->GetNodeForHandle(tab->GetHandle());
    CHECK(selected_node);
    auto* slot_view = &GetOrCreateSlotViewForNode(*selected_node);
    slot_view->SetBoundsRect(selected_node->view()->GetLocalBounds());
    dragged_views.push_back(slot_view);
    if (selected_node == &node) {
      source_dragged_view = slot_view;
      list_selection_model.set_active(index);
    }
  }
  dragged_views.shrink_to_fit();

  CHECK(source_dragged_view);

  const gfx::Point offset_from_source = event.location();
  if (drag_controller_->Init(this, source_dragged_view, dragged_views,
                             offset_from_source, list_selection_model,
                             EventSourceFromEvent(event)) ==
      TabDragController::Liveness::kDeleted) {
    ResetDragState();
  }
}

bool VerticalTabDragHandlerImpl::ContinueDrag(views::View& event_source_view,
                                              const ui::MouseEvent& event) {
  if (!drag_controller_) {
    return false;
  }
  gfx::Point screen_location(event.location());
  ConvertPointToScreen(&event_source_view, &screen_location);
  if (drag_controller_->Drag(screen_location) ==
      TabDragController::Liveness::kDeleted) {
    ResetDragState();
    return false;
  }
  return true;
}

void VerticalTabDragHandlerImpl::EndDrag(EndDragReason reason) {
  if (TabDragController::IsSystemDnDSessionRunning()) {
    TabDragController::OnSystemDnDEnded();
  } else if (drag_controller_ && drag_controller_->started_drag()) {
    drag_controller_->EndDrag(reason);
  }
  ResetDragState();
}

void VerticalTabDragHandlerImpl::HandleDraggedTabsOverNode(
    const TabCollectionNode& node) {
  if (!drag_controller_) {
    // Do nothing if the drag is not attached to our context yet (e.g. on the
    // first iteration of the drag loop).
    return;
  }
  switch (node.type()) {
    case TabCollectionNode::Type::TAB:
      HandleTabDragOverTab(node);
      break;
    case TabCollectionNode::Type::SPLIT:
      HandleTabDragOverSplit(node);
      break;
    case TabCollectionNode::Type::GROUP:
      HandleTabDragOverGroup(node);
      break;
    case TabCollectionNode::Type::UNPINNED:
      HandleTabDragOverUnpinnedContainer(node);
      break;
    default:
      NOTREACHED();
  }
}

void VerticalTabDragHandlerImpl::HandleTabDragOverTab(
    const TabCollectionNode& node) {
  const auto* tab = std::get<const tabs::TabInterface*>(node.GetNodeData());
  CHECK(tab);
  const auto& selection_model = tab_strip_model_->selection_model();
  int first_selected_idx =
      *selection_model.GetListSelectionModel().selected_indices().cbegin();
  int insertion_idx = tab_strip_model_->GetIndexOfTab(tab);
  if (first_selected_idx <= insertion_idx) {
    insertion_idx -= selection_model.size();
    ++insertion_idx;
  }
  insertion_idx = std::clamp(insertion_idx, 0, tab_strip_model_->count() - 1);
  tab_strip_model_->MoveSelectedTabsTo(insertion_idx, tab->GetGroup());
}

void VerticalTabDragHandlerImpl::HandleTabDragOverSplit(
    const TabCollectionNode& node) {
  const auto* split_collection = static_cast<const tabs::SplitTabCollection*>(
      std::get<const tabs::TabCollection*>(node.GetNodeData()));
  CHECK(split_collection);
  split_tabs::SplitTabData* split_data = split_collection->data();
  CHECK(split_data);
  gfx::Range tab_range = split_data->GetIndexRange();
  int first_tab_in_split = tab_range.GetMin();
  int last_tab_in_split = tab_range.GetMax();

  const auto& selection_model = tab_strip_model_->selection_model();
  int first_selected_index =
      *selection_model.GetListSelectionModel().selected_indices().cbegin();
  int insertion_idx =
      (first_selected_index < first_tab_in_split)
          ? last_tab_in_split - selection_model.selected_tabs().size()
          : first_tab_in_split;

  tab_strip_model_->MoveSelectedTabsTo(
      insertion_idx, split_data->ListTabs().front()->GetGroup());
}

void VerticalTabDragHandlerImpl::HandleTabDragOverGroup(
    const TabCollectionNode& node) {
  const auto* tab_group =
      static_cast<const tabs::TabGroupTabCollection*>(
          std::get<const tabs::TabCollection*>(node.GetNodeData()))
          ->GetTabGroup();
  CHECK(tab_group);

  const auto& selection_model = tab_strip_model_->selection_model();

  if (std::all_of(selection_model.selected_tabs().begin(),
                  selection_model.selected_tabs().end(),
                  [tab_group](const tabs::TabInterface* selected_tab) {
                    return selected_tab->GetGroup() == tab_group->id();
                  })) {
    // Selected tabs are already in the group, so return early.
    return;
  }

  int first_tab_in_group =
      tab_strip_model_->GetIndexOfTab(tab_group->GetFirstTab());
  int last_tab_in_group =
      tab_strip_model_->GetIndexOfTab(tab_group->GetLastTab());
  int first_selected_index =
      *selection_model.GetListSelectionModel().selected_indices().cbegin();

  if (tab_strip_model_->IsGroupCollapsed(tab_group->id())) {
    // Selected tabs need to be inserted outside the group if collapsed.
    int insertion_idx = (first_selected_index < first_tab_in_group)
                            ? last_tab_in_group
                            : first_tab_in_group;
    tab_strip_model_->MoveSelectedTabsTo(insertion_idx, std::nullopt);
  } else {
    int insertion_idx =
        (first_selected_index < first_tab_in_group)
            ? first_tab_in_group - selection_model.selected_tabs().size()
            : last_tab_in_group + 1;
    insertion_idx = std::clamp(insertion_idx, 0, tab_strip_model_->count() - 1);
    tab_strip_model_->MoveSelectedTabsTo(insertion_idx, tab_group->id());
  }
}

void VerticalTabDragHandlerImpl::HandleTabDragOverUnpinnedContainer(
    const TabCollectionNode& node) {
  const tabs::TabInterface* selected_tab =
      *tab_strip_model_->selection_model().selected_tabs().cbegin();

  if (selected_tab->GetGroup().has_value()) {
    ui::ListSelectionModel::SelectedIndices selected =
        tab_strip_model_->selection_model()
            .GetListSelectionModel()
            .selected_indices();
    std::vector<int> tab_indices(selected.begin(), selected.end());
    tab_strip_model_->RemoveFromGroup(tab_indices);
  }
}

TabDragContext* VerticalTabDragHandlerImpl::GetDragContext() {
  return this;
}

bool VerticalTabDragHandlerImpl::IsViewDragging(const views::View& view) const {
  if (!drag_controller_) {
    return false;
  }
  for (TabSlotView* slot_view :
       drag_controller_->GetSessionData().attached_views()) {
    if (&view == ViewFromTabSlot(slot_view)) {
      return true;
    }
  }
  return false;
}

bool VerticalTabDragHandlerImpl::IsDraggingPinnedTabs() const {
  if (!drag_controller_) {
    return false;
  }
  const auto& drag_data = drag_controller_->GetSessionData().tab_drag_data_;
  return std::any_of(drag_data.cbegin(), drag_data.cend(),
                     [](const auto& tab_data) { return tab_data.pinned; });
}

views::View* VerticalTabDragHandlerImpl::ViewFromTabSlot(
    TabSlotView* view) const {
  auto* slot_view = views::AsViewClass<VerticalTabSlotView>(view);
  CHECK(slot_view);

  const TabCollectionNode& node = slot_view->node();

  // If the dragged tab view is in a split, return the split's tab view
  // instead.
  if (node.type() == TabCollectionNode::Type::TAB) {
    const auto* tab = std::get<const tabs::TabInterface*>(node.GetNodeData());
    CHECK(tab);
    if (tab->IsSplit()) {
      const TabCollectionNode* split_node =
          root_node_->GetParentNodeForHandle(tab->GetHandle());
      CHECK(split_node);
      CHECK_EQ(split_node->type(), TabCollectionNode::Type::SPLIT);
      return split_node->view();
    }
  }

  return node.view();
}

bool VerticalTabDragHandlerImpl::CanAcceptEvent(const ui::Event& event) {
  // The drag context has to be able to process mouse events during the drag.
  // By default, this is predicated on visibility, but the handler should not
  // be visible. Instead, defer the check to the parent.
  return parent()->CanAcceptEvent(event);
}

bool VerticalTabDragHandlerImpl::OnMouseDragged(const ui::MouseEvent& event) {
  return ContinueDrag(*this, event);
}

void VerticalTabDragHandlerImpl::OnMouseReleased(const ui::MouseEvent& event) {
  EndDrag(EndDragReason::kComplete);
}

void VerticalTabDragHandlerImpl::OnMouseCaptureLost() {
  EndDrag(EndDragReason::kCaptureLost);
}

TabDragContext* VerticalTabDragHandlerImpl::GetContextForNewBrowser(
    BrowserView* browser_view) const {
  return browser_view->tab_strip_view()->GetDragContext();
}

TabSlotView* VerticalTabDragHandlerImpl::GetTabForContents(
    content::WebContents* contents) {
  TabCollectionNode* node = GetNodeForContents(contents);
  return node ? &GetOrCreateSlotViewForNode(*node) : nullptr;
}

content::WebContents* VerticalTabDragHandlerImpl::GetContentsForTab(
    TabSlotView* view) {
  auto* slot_view = views::AsViewClass<VerticalTabSlotView>(view);
  CHECK(slot_view);
  // TODO(crbug.com/439963720): Support dragging other types.
  CHECK(slot_view->node().type() == TabCollectionNode::Type::TAB);
  const tabs::TabInterface* tab =
      std::get<const tabs::TabInterface*>(slot_view->node().GetNodeData());
  CHECK(tab);
  return tab->GetContents();
}

bool VerticalTabDragHandlerImpl::IsTabDetachable(
    const TabSlotView* view) const {
  return true;
}

TabSlotView* VerticalTabDragHandlerImpl::GetTabGroupHeader(
    const tab_groups::TabGroupId& group) const {
  // TODO(crbug.com/439963720): Support dragging tab groups.
  return nullptr;
}

TabStripModel* VerticalTabDragHandlerImpl::GetTabStripModel() {
  return base::to_address(tab_strip_model_);
}

TabDragController* VerticalTabDragHandlerImpl::GetDragController() {
  return drag_controller_.get();
}

void VerticalTabDragHandlerImpl::OwnDragController(
    std::unique_ptr<TabDragController> controller) {
  drag_controller_ = std::move(controller);
}

std::unique_ptr<TabDragController>
VerticalTabDragHandlerImpl::ReleaseDragController() {
  return std::move(drag_controller_);
}

void VerticalTabDragHandlerImpl::DestroyDragController() {
  drag_controller_.reset();
}

void VerticalTabDragHandlerImpl::StartedDragging(
    const std::vector<TabSlotView*>& views) {
  for (auto* view : views) {
    auto* slot_view = views::AsViewClass<VerticalTabSlotView>(view);
    CHECK(slot_view);

    views::View* dragged_view = ViewFromTabSlot(slot_view);
    CHECK(dragged_view);
    dragged_view->SetPaintToLayer();
    dragged_view->layer()->SetFillsBoundsOpaquely(false);

    // Update the height to use preferred size because newly added tabs will
    // animate in from 0, which affects the window offset for newly-detached
    // windows.
    gfx::Rect bounds = slot_view->node().view()->GetLocalBounds();
    bounds.set_height(slot_view->node().view()->GetPreferredSize({}).height());
    slot_view->SetBoundsRect(bounds);
  }
}

void VerticalTabDragHandlerImpl::DraggedTabsDetached() {}

void VerticalTabDragHandlerImpl::StoppedDragging() {
  for (auto& [_, slot_view] : slot_views_) {
    views::View* dragged_view = ViewFromTabSlot(slot_view);
    CHECK(dragged_view);
    dragged_view->DestroyLayer();
  }
}

void VerticalTabDragHandlerImpl::SetDragControllerCallbackForTesting(
    base::OnceCallback<void(TabDragController*)> callback) {}

TabDragPositioningDelegate*
VerticalTabDragHandlerImpl::GetPositioningDelegate() {
  // Positioning is implemented through `TabDragDelegate` on individual
  // containers.
  return nullptr;
}

TabCollectionNode* VerticalTabDragHandlerImpl::GetNodeForContents(
    content::WebContents* contents) {
  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(contents);
  CHECK(tab);
  return root_node_->GetNodeForHandle(tab->GetHandle());
}

TabSlotView& VerticalTabDragHandlerImpl::GetOrCreateSlotViewForNode(
    TabCollectionNode& node) {
  CHECK(node.view());
  auto it = slot_views_.find(&node);
  if (it != slot_views_.end()) {
    return *it->second;
  }

  auto tab_slot_view = std::make_unique<VerticalTabSlotView>(node);
  tab_slot_view->SetBoundsRect(node.view()->GetLocalBounds());
  auto& tab_slot_view_ref = *tab_slot_view.get();
  slot_views_.insert(
      {&node, node.view()->AddChildView(std::move(tab_slot_view))});
  node_destroyed_callbacks_.push_back(node.RegisterWillDestroyCallback(
      base::BindOnce(&VerticalTabDragHandlerImpl::OnNodeWillDestroy,
                     base::Unretained(this), std::ref(node))));
  tab_slot_view_ref.SetVisible(false);
  return tab_slot_view_ref;
}

void VerticalTabDragHandlerImpl::OnNodeWillDestroy(TabCollectionNode& node) {
  auto it = slot_views_.find(&node);
  CHECK(it != slot_views_.end());
  auto view = node.view()->RemoveChildViewT(it->second);
  slot_views_.erase(it);
}

void VerticalTabDragHandlerImpl::ResetDragState() {
  drag_controller_.reset();
}

BEGIN_METADATA(VerticalTabDragHandlerImpl)
END_METADATA
