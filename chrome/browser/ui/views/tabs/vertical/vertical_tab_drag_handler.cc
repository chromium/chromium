// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_drag_handler.h"

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
#include "components/tabs/public/tab_interface.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view_utils.h"

namespace {

// This is a shim around `TabCollectionNode` to make it compatible with the
// tab dragging logic in `TabDragController`. Longer term, core tab dragging
// logic will be updated to remove the `TabSlotView` dependency, which
// should make this shim view no longer needed.
class TabSlotShimView : public TabSlotView {
  METADATA_HEADER(TabSlotShimView, TabSlotView)
 public:
  explicit TabSlotShimView(TabCollectionNode& node) : node_(node) {
    // TODO(crbug.com/439963720): Support dragging other types.
    CHECK(node_->type() == TabCollectionNode::Type::TAB);
  }

  ~TabSlotShimView() override = default;
  TabSlotShimView(const TabSlotShimView&) = delete;
  TabSlotShimView& operator=(const TabSlotShimView&) = delete;

  TabSlotView::ViewType GetTabSlotViewType() const override {
    return ViewType::kTab;
  }

  TabSizeInfo GetTabSizeInfo() const override { return TabSizeInfo(); }

  const TabCollectionNode& node() const { return node_.get(); }

 private:
  const raw_ref<TabCollectionNode> node_;
};

BEGIN_METADATA(TabSlotShimView)
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

  // TODO(crbug.com/439963720): Support dragging multiple tabs.
  dragged_tabs_.insert(&node);
  const gfx::Point offset_from_first_dragged_view = event.location();
  const gfx::Point offset_from_source = event.location();
  ui::ListSelectionModel selection_model;
  TabSlotView& dragged_view = GetOrCreateShimViewForNode(node);

  if (drag_controller_->Init(this, &dragged_view, {&dragged_view},
                             offset_from_first_dragged_view, offset_from_source,
                             std::move(selection_model),
                             EventSourceFromEvent(event)) ==
      TabDragController::Liveness::kDeleted) {
    dragged_tabs_.clear();
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

TabDragContext* VerticalTabDragHandlerImpl::GetDragContext() {
  return this;
}

bool VerticalTabDragHandlerImpl::OnMouseDragged(const ui::MouseEvent& event) {
  return ContinueDrag(*this, event);
}

void VerticalTabDragHandlerImpl::OnMouseReleased(const ui::MouseEvent& event) {
  EndDrag(EndDragReason::kComplete);
}

TabDragContext* VerticalTabDragHandlerImpl::GetContextForNewBrowser(
    BrowserView* browser_view) const {
  return browser_view->tab_strip_view()->GetDragContext();
}

TabSlotView* VerticalTabDragHandlerImpl::GetTabForContents(
    content::WebContents* contents) {
  TabCollectionNode* node = GetNodeForContents(contents);
  return node ? &GetOrCreateShimViewForNode(*node) : nullptr;
}

content::WebContents* VerticalTabDragHandlerImpl::GetContentsForTab(
    TabSlotView* view) {
  auto* shim_view = views::AsViewClass<TabSlotShimView>(view);
  CHECK(shim_view);
  // TODO(crbug.com/439963720): Support dragging other types.
  CHECK(shim_view->node().type() == TabCollectionNode::Type::TAB);
  const tabs::TabInterface* tab =
      std::get<const tabs::TabInterface*>(shim_view->node().GetNodeData());
  CHECK(tab);
  return tab->GetContents();
}

bool VerticalTabDragHandlerImpl::IsTabPinned(const TabSlotView* tab) const {
  // TODO(crbug.com/439963720): Support dragging pinned tabs.
  return false;
}

bool VerticalTabDragHandlerImpl::IsTabDetachable(
    const TabSlotView* view) const {
  return true;
}

int VerticalTabDragHandlerImpl::GetTabCount() const {
  return dragged_tabs_.size();
}

int VerticalTabDragHandlerImpl::GetPinnedTabCount() const {
  // TODO(crbug.com/439963720): Support dragging pinned tabs.
  return 0;
}

TabGroupHeader* VerticalTabDragHandlerImpl::GetTabGroupHeader(
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
    const std::vector<TabSlotView*>& views) {}

void VerticalTabDragHandlerImpl::DraggedTabsDetached() {
  dragged_tabs_.clear();
}

void VerticalTabDragHandlerImpl::StoppedDragging() {
  dragged_tabs_.clear();
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

TabSlotView& VerticalTabDragHandlerImpl::GetOrCreateShimViewForNode(
    TabCollectionNode& node) {
  CHECK(node.view());
  auto it = shim_views_.find(&node);
  if (it != shim_views_.end()) {
    return *it->second;
  }

  auto tab_shim_view = std::make_unique<TabSlotShimView>(node);
  tab_shim_view->SetBoundsRect(node.view()->GetLocalBounds());
  auto& tab_shim_view_ref = *tab_shim_view.get();
  shim_views_.insert(
      {&node, node.view()->AddChildView(std::move(tab_shim_view))});
  node_destroyed_callbacks_.push_back(node.RegisterWillDestroyCallback(
      base::BindOnce(&VerticalTabDragHandlerImpl::OnNodeWillDestroy,
                     base::Unretained(this), std::ref(node))));
  return tab_shim_view_ref;
}

void VerticalTabDragHandlerImpl::OnNodeWillDestroy(TabCollectionNode& node) {
  auto it = shim_views_.find(&node);
  CHECK(it != shim_views_.end());
  auto view = node.view()->RemoveChildViewT(it->second);
  shim_views_.erase(it);
  dragged_tabs_.erase(&node);
}

void VerticalTabDragHandlerImpl::ResetDragState() {
  drag_controller_.reset();
  dragged_tabs_.clear();
}

BEGIN_METADATA(VerticalTabDragHandlerImpl)
END_METADATA
