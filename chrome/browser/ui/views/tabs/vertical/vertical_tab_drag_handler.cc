// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_drag_handler.h"

#include <algorithm>
#include <memory>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/types/to_address.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/dragging/drag_session_data.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_controller.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip_types.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_link_drop_handler.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_controller.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/split_tab_collection.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/class_property.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/view_utils.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(gfx::Vector2d*)

namespace {

// Stores the offset of the view's initial position to the target position
// it should be, relative to the source dragged view's origin.
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Vector2d, kOffsetAtTabDragStart)

// This is a shim around `TabCollectionNode` to make it compatible with the
// tab dragging logic in `TabDragController`. Longer term, core tab dragging
// logic will be updated to remove the `TabSlotView` dependency, which
// should make this shim view no longer needed.
class VerticalTabSlotView : public TabSlotView {
  METADATA_HEADER(VerticalTabSlotView, TabSlotView)
 public:
  explicit VerticalTabSlotView(const TabCollectionNode& node) : node_(node) {}

  ~VerticalTabSlotView() override = default;
  VerticalTabSlotView(const VerticalTabSlotView&) = delete;
  VerticalTabSlotView& operator=(const VerticalTabSlotView&) = delete;

  TabSlotView::ViewType GetTabSlotViewType() const override {
    switch (node_->type()) {
      case TabCollectionNode::Type::TAB:
        return ViewType::kTab;
      case TabCollectionNode::Type::GROUP:
        return ViewType::kTabGroupHeader;
      default:
        NOTREACHED();
    }
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

const TabGroup& TabGroupDataFromNode(const TabCollectionNode& node) {
  CHECK_EQ(node.type(), TabCollectionNode::Type::GROUP);
  const auto* collection = static_cast<const tabs::TabGroupTabCollection*>(
      std::get<const tabs::TabCollection*>(node.GetNodeData()));
  CHECK(collection);
  const auto* group_data = collection->GetTabGroup();
  CHECK(group_data);
  return *group_data;
}

}  // namespace

VerticalTabDragHandlerImpl::VerticalTabDragHandlerImpl(
    TabStripModel& tab_strip_model,
    TabCollectionNode& root_node)
    : tab_strip_model_(tab_strip_model),
      root_node_(root_node),
      link_drop_handler_(
          std::make_unique<VerticalTabLinkDropHandler>(tab_strip_model)) {}

VerticalTabDragHandlerImpl::~VerticalTabDragHandlerImpl() = default;

void VerticalTabDragHandlerImpl::InitializeDrag(TabCollectionNode& node,
                                                const ui::MouseEvent& event) {
  // TODO(crbug.com/439963720): Look into why the state is not reset elsewhere
  // after initializing a drag.
  ResetDragState();
  drag_controller_ = std::make_unique<TabDragController>();

  DragInitData drag_init_data;
  switch (node.type()) {
    case TabCollectionNode::Type::TAB:
      drag_init_data = GetDragInitDataForTabDrag(node);
      break;
    case TabCollectionNode::Type::GROUP:
      drag_init_data = GetDragInitDataForGroupHeaderDrag(node);
      break;
    default:
      NOTREACHED();
  }

  CHECK(drag_init_data.source_dragged_view);

  const gfx::Point offset_from_source = event.location();
  if (drag_controller_->Init(this, drag_init_data.source_dragged_view,
                             drag_init_data.dragged_views, offset_from_source,
                             drag_init_data.list_selection_model,
                             EventSourceFromEvent(event)) ==
      TabDragController::Liveness::kDeleted) {
    ResetDragState();
  }
}

VerticalTabDragHandlerImpl::DragInitData::DragInitData() = default;
VerticalTabDragHandlerImpl::DragInitData::~DragInitData() = default;
VerticalTabDragHandlerImpl::DragInitData::DragInitData(
    const VerticalTabDragHandlerImpl::DragInitData&) = default;
VerticalTabDragHandlerImpl::DragInitData&
VerticalTabDragHandlerImpl::DragInitData::operator=(
    const VerticalTabDragHandlerImpl::DragInitData&) = default;

VerticalTabDragHandlerImpl::DragInitData
VerticalTabDragHandlerImpl::GetDragInitDataForTabDrag(
    TabCollectionNode& source_node) {
  const auto& selected_tabs_indices =
      tab_strip_model_->selection_model().GetListSelectionModel();
  std::vector<tabs::TabInterface*> selected_tabs;
  for (int index : selected_tabs_indices.selected_indices()) {
    selected_tabs.push_back(tab_strip_model_->GetTabAtIndex(index));
  }
  DragInitData drag_init_data;
  drag_init_data.dragged_views.reserve(selected_tabs.size());

  const auto* source_tab =
      std::get<const tabs::TabInterface*>(source_node.GetNodeData());
  CHECK(source_tab);

  std::map<tab_groups::TabGroupId, TabSlotView*> dragged_groups;
  if (!source_tab->IsPinned()) {
    // A TabSlotView must be added for each dragged group, in addition to the
    // tabs themselves. These must be inserted into `dragged_views` in the
    // order they appear within the tab strip.
    dragged_groups = GetFullySelectedGroups(selected_tabs);
  }

  // Track the node and build a shim view for each selected node.
  for (tabs::TabInterface* tab : selected_tabs) {
    // Filter out selections that don't match the pinned state of the latest
    // selected tab.
    if (source_tab->IsPinned() != tab->IsPinned()) {
      continue;
    }

    if (auto group_id = tab->GetGroup()) {
      // If the tab belongs to a group that is fully selected, then add the
      // group's TabSlotView first.
      if (auto it = dragged_groups.find(*group_id);
          it != dragged_groups.end()) {
        drag_init_data.dragged_views.push_back(it->second);
        dragged_groups.erase(it);
      }
    }

    size_t index = tab_strip_model_->GetIndexOfTab(tab);
    drag_init_data.list_selection_model.AddIndexToSelection(index);
    TabCollectionNode* selected_node =
        root_node_->GetNodeForHandle(tab->GetHandle());
    CHECK(selected_node);
    auto* slot_view = &GetOrCreateSlotViewForNode(*selected_node);
    slot_view->SetBoundsRect(selected_node->view()->GetLocalBounds());
    drag_init_data.dragged_views.push_back(slot_view);
    if (selected_node == &source_node) {
      drag_init_data.source_dragged_view = slot_view;
      drag_init_data.list_selection_model.set_active(index);
    }
  }
  drag_init_data.dragged_views.shrink_to_fit();
  return drag_init_data;
}

VerticalTabDragHandlerImpl::DragInitData
VerticalTabDragHandlerImpl::GetDragInitDataForGroupHeaderDrag(
    TabCollectionNode& source_node) {
  DragInitData drag_init_data;
  drag_init_data.source_dragged_view = &GetOrCreateSlotViewForNode(source_node);
  drag_init_data.dragged_views.push_back(drag_init_data.source_dragged_view);
  for (const auto& child : source_node.children()) {
    if (child->type() == TabCollectionNode::Type::SPLIT) {
      for (const auto& split_child : child->children()) {
        drag_init_data.dragged_views.push_back(
            &GetOrCreateSlotViewForNode(*split_child.get()));
      }
    } else {
      drag_init_data.dragged_views.push_back(
          &GetOrCreateSlotViewForNode(*child.get()));
    }
  }
  return drag_init_data;
}

std::map<tab_groups::TabGroupId, TabSlotView*>
VerticalTabDragHandlerImpl::GetFullySelectedGroups(
    const std::vector<tabs::TabInterface*>& selected_tabs) {
  std::map<tab_groups::TabGroupId, int> dragged_group_tab_counts;
  for (tabs::TabInterface* tab : selected_tabs) {
    if (auto group_id = tab->GetGroup()) {
      dragged_group_tab_counts[*group_id]++;
    }
  }

  std::map<tab_groups::TabGroupId, TabSlotView*> selected_groups;
  for (const auto& [group_id, dragged_tab_count] : dragged_group_tab_counts) {
    const auto* group = tab_strip_model_->group_model()->GetTabGroup(group_id);
    CHECK(group);
    if (group->tab_count() == dragged_tab_count) {
      auto* selected_node = GetNodeForTabGroup(group_id);
      auto& slot_view = GetOrCreateSlotViewForNode(*selected_node);
      slot_view.SetBoundsRect(selected_node->view()->GetLocalBounds());
      selected_groups.insert({group_id, &slot_view});
    }
  }
  return selected_groups;
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
    return;
  }

  // Let TabDragController decide whether this reason should actually end the
  // drag (e.g. capture loss while dragging a detached window) and destroy
  // itself when appropriate.
  if (drag_controller_) {
    drag_controller_->EndDrag(reason);
  }
}

void VerticalTabDragHandlerImpl::HandleDraggedTabsOverNode(
    const TabCollectionNode& node,
    std::optional<DragPositionHint> position_hint) {
  CHECK(drag_controller_);

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
    default:
      NOTREACHED();
  }
}

void VerticalTabDragHandlerImpl::HandleDraggedTabsIntoNode(
    const TabCollectionNode& node) {
  CHECK(drag_controller_);
  const auto& drag_session_data = drag_controller_->GetSessionData();

  // Do nothing if the group is not being changed and either one tab is being
  // dragged or all tabs in the strip are being dragged.
  if (node.type() != TabCollectionNode::Type::GROUP &&
      (drag_session_data.num_dragging_tabs() == 1 ||
       drag_session_data.num_dragging_tabs() == tab_strip_model_->count())) {
    return;
  }

  const TabDragData* source_drag_data =
      drag_session_data.source_view_drag_data();
  const content::WebContents* source_contents = source_drag_data->contents;
  if (!source_contents) {
    return;
  }

  int target_index;
  std::optional<tab_groups::TabGroupId> target_group_id = std::nullopt;
  if (node.type() == TabCollectionNode::Type::GROUP) {
    // If dragging into a group, then either put the dragged tabs into the
    // start/end if the source dragged tab is before/after the group, or move
    // all dragged tabs to the source dragged tab's position if it's already
    // in the group.
    const auto& group = TabGroupDataFromNode(node);
    target_group_id = group.id();
    int source_tab_index =
        tab_strip_model_->GetIndexOfWebContents(source_contents);
    auto group_indexes = group.ListTabs();
    if (source_tab_index < static_cast<int>(group_indexes.start())) {
      target_index = group_indexes.start();
    } else if (source_tab_index > static_cast<int>(group_indexes.end())) {
      target_index = group_indexes.end();
    } else {
      target_index = source_tab_index;
    }
  } else if (auto source_tab_group_id =
                 tabs::TabInterface::GetFromContents(source_contents)
                     ->GetGroup()) {
    // If the source dragged tab is in a group, and we're not entering a
    // group, then move the dragged tabs to be after the group.
    const auto* group =
        tab_strip_model_->group_model()->GetTabGroup(*source_tab_group_id);
    target_index = group->ListTabs().end();
  } else {
    target_index = tab_strip_model_->GetIndexOfWebContents(source_contents);
  }

  int num_dragged_tabs_before_target = 0;
  for (const TabDragData& tab_drag_data : drag_session_data.tab_drag_data_) {
    const content::WebContents* contents = tab_drag_data.contents;
    if (contents &&
        tab_strip_model_->GetIndexOfWebContents(contents) < target_index) {
      ++num_dragged_tabs_before_target;
    }
  }
  target_index = target_index - num_dragged_tabs_before_target;
  target_index = std::clamp(target_index, 0, tab_strip_model_->count() - 1);
  tab_strip_model_->MoveSelectedTabsTo(target_index, target_group_id);
}

void VerticalTabDragHandlerImpl::HandleDraggedTabsOutOfGroup(
    const TabCollectionNode& node,
    DragPositionHint position_hint) {
  CHECK_EQ(node.type(), TabCollectionNode::Type::GROUP);

  const auto& tab_group = TabGroupDataFromNode(node);

  const auto& selection_model = tab_strip_model_->selection_model();
  int insertion_idx;
  switch (position_hint) {
    case DragPositionHint::kAfter: {
      int last_tab_in_group =
          tab_strip_model_->GetIndexOfTab(tab_group.GetLastTab());
      insertion_idx =
          last_tab_in_group - selection_model.selected_tabs().size() + 1;
      break;
    }
    case DragPositionHint::kBefore: {
      int first_tab_in_group =
          tab_strip_model_->GetIndexOfTab(tab_group.GetFirstTab());
      insertion_idx = first_tab_in_group;
      break;
    }
    default:
      NOTREACHED();
  }

  insertion_idx = std::clamp(insertion_idx, 0, tab_strip_model_->count() - 1);
  tab_strip_model_->MoveSelectedTabsTo(insertion_idx, std::nullopt);
}

void VerticalTabDragHandlerImpl::HandleDraggedTabsAtEndOfTabStrip() {
  // If the tabs were dragging into the tab strip in an area where they did not
  // overlap any nodes then update the model appropriately if the tabs are not
  // already at the end.
  if (!IsDraggingAtEndOfTabStrip()) {
    tab_strip_model_->MoveSelectedTabsTo(tab_strip_model_->count() - 1,
                                         std::nullopt);
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
  if (auto group = GetDraggingGroupHeaderId(); group.has_value()) {
    tab_strip_model_->MoveGroupTo(*group, insertion_idx);
  } else {
    tab_strip_model_->MoveSelectedTabsTo(insertion_idx, tab->GetGroup());
  }
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

  if (auto group = GetDraggingGroupHeaderId(); group.has_value()) {
    tab_strip_model_->MoveGroupTo(*group, insertion_idx);
  } else {
    tab_strip_model_->MoveSelectedTabsTo(
        insertion_idx, split_data->ListTabs().front()->GetGroup());
  }
}

void VerticalTabDragHandlerImpl::HandleTabDragOverGroup(
    const TabCollectionNode& node) {
  const auto* tab_group =
      static_cast<const tabs::TabGroupTabCollection*>(
          std::get<const tabs::TabCollection*>(node.GetNodeData()))
          ->GetTabGroup();
  CHECK(tab_group);
  const auto& selection_model = tab_strip_model_->selection_model();

  int first_tab_in_group =
      tab_strip_model_->GetIndexOfTab(tab_group->GetFirstTab());
  int last_tab_in_group =
      tab_strip_model_->GetIndexOfTab(tab_group->GetLastTab());
  int first_selected_index =
      *selection_model.GetListSelectionModel().selected_indices().cbegin();

  // If dragging over a collapsed group or dragging a group, then
  // move the dragged tabs/header before or after the dragged-over group.
  int insertion_idx =
      (first_selected_index < first_tab_in_group)
          ? last_tab_in_group - selection_model.selected_tabs().size() + 1
          : first_tab_in_group;
  if (auto dragged_group = GetDraggingGroupHeaderId()) {
    tab_strip_model_->MoveGroupTo(*dragged_group, insertion_idx);
  } else {
    tab_strip_model_->MoveSelectedTabsTo(insertion_idx, std::nullopt);
  }
}

TabDragContext* VerticalTabDragHandlerImpl::GetDragContext() {
  return this;
}

bool VerticalTabDragHandlerImpl::IsDragging() const {
  return drag_controller_ && drag_controller_->active();
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

bool VerticalTabDragHandlerImpl::IsDraggingGroups() const {
  if (!drag_controller_) {
    return false;
  }
  return !drag_controller_->GetSessionData().dragging_groups.empty();
}

bool VerticalTabDragHandlerImpl::IsDraggingAtEndOfTabStrip() const {
  if (!drag_controller_) {
    return false;
  }
  const auto& drag_data = drag_controller_->GetSessionData().tab_drag_data_;
  const auto* last_web_contents =
      tab_strip_model_->GetWebContentsAt(tab_strip_model_->count() - 1);
  return std::any_of(drag_data.cbegin(), drag_data.cend(),
                     [last_web_contents](const auto& tab_data) {
                       return tab_data.contents == last_web_contents;
                     });
}

std::optional<tab_groups::TabGroupId>
VerticalTabDragHandlerImpl::GetDraggingGroupHeaderId() const {
  return drag_controller_ ? drag_controller_->GetSessionData().group_header_id()
                          : std::nullopt;
}

views::View* VerticalTabDragHandlerImpl::ViewFromTabSlot(
    TabSlotView* view) const {
  CHECK(drag_controller_);
  auto* slot_view = views::AsViewClass<VerticalTabSlotView>(view);
  CHECK(slot_view);

  const TabCollectionNode& node = slot_view->node();

  if (node.type() == TabCollectionNode::Type::TAB) {
    const auto* tab = std::get<const tabs::TabInterface*>(node.GetNodeData());
    CHECK(tab);

    if (auto group_id = tab->GetGroup();
        group_id && drag_controller_->GetSessionData().dragging_groups.contains(
                        *group_id)) {
      // If the tab belongs to a group that is being dragged, return the group's
      // view instead.
      const TabCollectionNode* group_node = GetNodeForTabGroup(*group_id);
      CHECK(group_node);
      return group_node->view();
    }

    if (tab->IsSplit()) {
      // If the dragged tab view is in a split, return the split's tab view
      // instead.
      const TabCollectionNode* split_node =
          root_node_->GetParentNodeForHandle(tab->GetHandle());
      CHECK(split_node);
      CHECK_EQ(split_node->type(), TabCollectionNode::Type::SPLIT);
      return split_node->view();
    }
  }

  return node.view();
}

std::optional<gfx::Vector2d>
VerticalTabDragHandlerImpl::GetOffsetFromSourceAtDragStart(View* view) const {
  gfx::Vector2d* offset = view->GetProperty(kOffsetAtTabDragStart);
  return offset ? std::make_optional(*offset) : std::nullopt;
}

std::optional<BrowserRootView::DropIndex>
VerticalTabDragHandlerImpl::GetLinkDropIndexForNode(
    const TabCollectionNode& node,
    std::optional<DragPositionHint> position_hint) const {
  return link_drop_handler_->GetDropIndexForNode(node, position_hint);
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

bool VerticalTabDragHandlerImpl::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  if (!TabDragController::IsSystemDnDSessionRunning()) {
    return false;
  }
  format_types->insert(
      ui::ClipboardFormatType::CustomPlatformType(ui::kMimeTypeWindowDrag));
  return true;
}

bool VerticalTabDragHandlerImpl::CanDrop(const OSExchangeData& data) {
  return TabDragController::IsSystemDnDSessionRunning() &&
         data.HasCustomFormat(ui::ClipboardFormatType::CustomPlatformType(
             ui::kMimeTypeWindowDrag));
}

void VerticalTabDragHandlerImpl::OnDragEntered(
    const ui::DropTargetEvent& event) {
  CHECK(TabDragController::IsSystemDnDSessionRunning());
  TabDragController::OnSystemDnDUpdated(event);
}
int VerticalTabDragHandlerImpl::OnDragUpdated(
    const ui::DropTargetEvent& event) {
  // This can be false because we can still receive drag events after
  // TabDragController is destroyed due to the asynchronous nature of the
  // platform DnD.
  if (TabDragController::IsSystemDnDSessionRunning()) {
    TabDragController::OnSystemDnDUpdated(event);
    return ui::DragDropTypes::DRAG_MOVE;
  }
  return ui::DragDropTypes::DRAG_NONE;
}

void VerticalTabDragHandlerImpl::OnDragExited() {
  // See comment in OnDragUpdated().
  if (TabDragController::IsSystemDnDSessionRunning()) {
    TabDragController::OnSystemDnDExited();
  }
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
  if (slot_view->node().type() != TabCollectionNode::Type::TAB) {
    return nullptr;
  }
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
    const tab_groups::TabGroupId& group_id) {
  TabCollectionNode* node = GetNodeForTabGroup(group_id);
  return node ? &GetOrCreateSlotViewForNode(*node) : nullptr;
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
  CHECK(drag_controller_);
  auto* source_dragged_view = ViewFromTabSlot(drag_controller_->GetSessionData()
                                                  .source_view_drag_data()
                                                  ->attached_view);
  CHECK(source_dragged_view);
  gfx::Point source_view_origin_in_screen =
      source_dragged_view->GetBoundsInScreen().origin();

  for (auto* view : views) {
    auto* slot_view = views::AsViewClass<VerticalTabSlotView>(view);
    CHECK(slot_view);

    views::View* dragged_view = ViewFromTabSlot(slot_view);
    CHECK(dragged_view);
    dragged_view->SetPaintToLayer();
    dragged_view->layer()->SetFillsBoundsOpaquely(false);
    gfx::Vector2d offset = dragged_view->GetBoundsInScreen().origin() -
                           source_view_origin_in_screen;
    dragged_view->SetProperty(kOffsetAtTabDragStart, offset);

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
    dragged_view->ClearProperty(kOffsetAtTabDragStart);
  }

  if (!drag_controller_) {
    return;
  }

  const DragSessionData& drag_data = drag_controller_->GetSessionData();
  if (!drag_data.group_header_drag_data_.has_value()) {
    return;
  }

  // Offset by 1 to account for the group header.
  const int drag_data_index =
      1 + drag_data.group_header_drag_data_->active_tab_index_within_group;
  const int index = tab_strip_model_->GetIndexOfWebContents(
      drag_data.tab_drag_data_[drag_data_index].contents);

  // The tabs in the group may have been closed during the drag.
  if (index != TabStripModel::kNoTab) {
    ui::ListSelectionModel selection;
    CHECK_GE(index, 0);
    selection.AddIndexToSelection(static_cast<size_t>(index));
    selection.set_active(static_cast<size_t>(index));
    selection.set_anchor(static_cast<size_t>(index));
    tab_strip_model_->SetSelectionFromModel(selection);
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

TabCollectionNode* VerticalTabDragHandlerImpl::GetNodeForTabGroup(
    const tab_groups::TabGroupId& group_id) {
  return const_cast<TabCollectionNode*>(
      std::as_const(*this).GetNodeForTabGroup(group_id));
}

const TabCollectionNode* VerticalTabDragHandlerImpl::GetNodeForTabGroup(
    const tab_groups::TabGroupId& group_id) const {
  const auto* group = tab_strip_model_->group_model()->GetTabGroup(group_id);
  CHECK(group);
  return root_node_->GetNodeForHandle(group->GetCollectionHandle());
}

TabSlotView& VerticalTabDragHandlerImpl::GetOrCreateSlotViewForNode(
    TabCollectionNode& node) {
  auto update_tab_slot_view = [&node](TabSlotView& slot_view) -> void {
    switch (node.type()) {
      case TabCollectionNode::Type::TAB: {
        const tabs::TabInterface* tab =
            std::get<const tabs::TabInterface*>(node.GetNodeData());
        CHECK(tab);
        slot_view.SetGroup(tab->GetGroup());
        slot_view.SetSplit(tab->GetSplit());
      } break;
      case TabCollectionNode::Type::GROUP:
        slot_view.SetGroup(TabGroupDataFromNode(node).id());
        break;
      default:
        NOTREACHED();
    }
    slot_view.SetBoundsRect(node.view()->GetLocalBounds());
  };

  CHECK(node.view());
  auto it = slot_views_.find(&node);
  if (it != slot_views_.end()) {
    update_tab_slot_view(*it->second);
    return *it->second;
  }

  auto tab_slot_view = std::make_unique<VerticalTabSlotView>(node);
  auto& tab_slot_view_ref = *tab_slot_view.get();
  update_tab_slot_view(tab_slot_view_ref);

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
