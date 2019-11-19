// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/viz/dom_agent_viz.h"

#include "base/stl_util.h"
#include "components/ui_devtools/root_element.h"
#include "components/ui_devtools/ui_element.h"
#include "components/ui_devtools/viz/frame_sink_element.h"
#include "components/ui_devtools/viz/surface_element.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface_manager.h"

namespace ui_devtools {

// Updating logic for FrameSinks:
//
// 1. Creating. We register a FrameSinkId, create a CompositorFrameSink and if a
// CompositorFrameSink is not a root we register hierarchy from the parent of
// this FrameSink to this CompositorFrameSink. When we register a FrameSinkId,
// we check if its corresponding element is already in the tree. If not, we
// attach it to the RootElement which serves as the root of the
// CompositorFrameSink tree. In this state the CompositorFrameSink is considered
// unembedded and it is a sibling of RootCompositorFrameSinks. If it is present
// in a tree we just change the properties (|has_created_frame_sink_|).
// These events don't know anything about the hierarchy
// so we don't change it. When we get OnRegisteredHierarchy from parent to child
// the corresponding elements must already be present in a tree. The usual state
// is: child is attached to RootElement and now we will detach it from the
// RootElement and attach to the real parent. During handling this event we
// actually delete the subtree of RootElement rooted from child and create a new
// subtree of parent. This potentially could be more efficient, but if we just
// switch necessary pointers we must send a notification to the frontend so it
// can update the UI. Unfortunately, this action involves deleting a node from
// the backend list of UI Elements (even when it is still alive) and trying to
// delete it once again (for instance, when we close a corresponding tab) causes
// crash.
//
// 2. Deleting. We unregister hierarchy, destroy a CompositorFrameSink and
// invalidate a FrameSinkId. When we invalidate an FrameSinkId or destroy a
// FrameSink we check if it's the last action that has to happen with the
// corresponding element. For example, if the element has
// |has_created_frame_sink_| = true and we get a |OnDestroyedFrameSink| event we
// just set |has_created_frame_sink_| = false, but don't remove it from a tree,
// because its FrameSinkId is still registered, so it's not completely dead. But
// when after that we get |OnInvalidatedFrameSinkId| we can remove the
// node from the tree. When we get OnUnregisteredHierarchy we assume the nodes
// are still present in a tree, so we do the same work as we did in registering
// case. Only here we move a subtree of parent rooted from child to the
// RootElement. Obviously, now the child will be in detached state.
//
// Updating logic for Surfaces:
// 1. Creating. We create a surface and then add reference to it.
// SurfaceManager::root_surface_id_ is treated like a regular surface id, so
// every time we add a reference from SurfaceManager::root_surface_id_ we add
// the child to the associated SurfaceElement.
//
// 2. Deleting. We remove the reference and destroy a Surface.
//
// Although this overview is relatively shorter than for FrameSinks and we have
// a smaller set of actions handling updates in the Surface tree is actually
// more difficult. Because of no particular order of the events we may face the
// cases when more than one surface references some surface or a surface doesn't
// remove its reference and just gets destroyed instead. The intermediate state
// between state A when surface hierarchy is a tree and state B when surface
// hierarchy is a tree again can be ambiguous and may not follow the tree
// structure. Current approach of handling these states needs revisiting.

DOMAgentViz::DOMAgentViz(viz::FrameSinkManagerImpl* frame_sink_manager)
    : frame_sink_manager_(frame_sink_manager),
      surface_manager_(frame_sink_manager->surface_manager()) {}

DOMAgentViz::~DOMAgentViz() {
  Clear();
}

void DOMAgentViz::OnFirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {
  // The surface was just created. No one embedded it yet, so just attach
  // it to RootElement. Sometimes OnAddedSurfaceReference is called first, so
  // don't create the element if it already exists.
  const viz::SurfaceId& surface_id = surface_info.id();
  if (!base::Contains(surface_elements_, surface_id)) {
    UIElement* surface_root = GetRootSurfaceElement();
    CreateSurfaceElement(surface_id, surface_root)
        ->AddToParentSorted(surface_root);
  }
}

bool DOMAgentViz::OnSurfaceDamaged(const viz::SurfaceId& surface_id,
                                   const viz::BeginFrameAck& ack) {
  return false;
}

void DOMAgentViz::OnSurfaceDestroyed(const viz::SurfaceId& surface_id) {
  // We may come across the case where we delete element, but its children
  // are still alive. Therefore we should attach children to the RootElement
  // and then delete this element.
  auto it = surface_elements_.find(surface_id);
  DCHECK(it != surface_elements_.end());
  DestroyElementAndRemoveSubtree(it->second.get());
}

// TODO(sgilhuly): Add support for elements to have multiple parents. Currently,
// when a reference is added to a surface, the SurfaceElement is moved to be a
// child of only its most recent referrer. When a reference is removed from a
// surface, this is ignored unless the reference is to the SurfaceElement's
// current parent.
void DOMAgentViz::OnAddedSurfaceReference(const viz::SurfaceId& parent_id,
                                          const viz::SurfaceId& child_id) {
  // Detach child element from its current parent and attach to the new parent.
  // OnAddedSurfaceReference is often called before OnFirstSurfaceActivation, so
  // create the element if it does not exist.
  auto it_parent = surface_elements_.find(parent_id);
  if (it_parent == surface_elements_.end()) {
    UIElement* surface_root = GetRootSurfaceElement();
    CreateSurfaceElement(parent_id, surface_root)
        ->AddToParentSorted(surface_root);
    // The subtree is populated in AddChild, so we don't need to do anything
    // else here.
    return;
  }
  UIElement* parent = it_parent->second.get();

  // It's possible that OnFirstSurfaceActivation hasn't been called yet, so
  // create a new element as a child of |parent| if it doesn't already exist.
  auto it_child = surface_elements_.find(child_id);
  if (it_child == surface_elements_.end()) {
    CreateSurfaceElement(child_id, parent)->AddToParentSorted(parent);
  } else {
    SurfaceElement* child = it_child->second.get();
    child->Reparent(parent);
  }
}

void DOMAgentViz::OnRemovedSurfaceReference(const viz::SurfaceId& parent_id,
                                            const viz::SurfaceId& child_id) {
  // Detach child element from its current parent and attach to the root
  // surface.
  auto it_child = surface_elements_.find(child_id);
  DCHECK(it_child != surface_elements_.end());
  SurfaceElement* child = it_child->second.get();

  // Do nothing if parent is not a parent of this child anymore. This can
  // happen when we have Surface A referencing Surface B, then we create
  // Surface C and ask it to reference Surface B. When A asks to remove
  // the reference to B, do nothing because B is already referenced by C.
  // TODO(sgilhuly): Add support for elements to have multiple parents so this
  // case can be correctly handled.
  UIElement* old_parent = child->parent();
  if (SurfaceElement::From(old_parent) != parent_id)
    return;

  child->Reparent(GetRootSurfaceElement());
}

void DOMAgentViz::OnRegisteredFrameSinkId(
    const viz::FrameSinkId& frame_sink_id) {
  // If a FrameSink was just registered we don't know anything about
  // hierarchy. So we should attach it to the RootElement.
  CreateFrameSinkElement(frame_sink_id, element_root(), /*is_root=*/false,
                         /*has_created_frame_sink=*/false)
      ->AddToParentSorted(element_root());
}

void DOMAgentViz::OnInvalidatedFrameSinkId(
    const viz::FrameSinkId& frame_sink_id) {
  auto it = frame_sink_elements_.find(frame_sink_id);
  DCHECK(it != frame_sink_elements_.end());

  // A FrameSinkElement with |frame_sink_id| can only be invalidated after
  // being destroyed.
  DCHECK(!it->second->has_created_frame_sink());
  DestroyElementAndRemoveSubtree(it->second.get());
}

void DOMAgentViz::OnCreatedCompositorFrameSink(
    const viz::FrameSinkId& frame_sink_id,
    bool is_root) {
  auto it = frame_sink_elements_.find(frame_sink_id);
  DCHECK(it != frame_sink_elements_.end());
  // The corresponding element is already present in a tree, so we
  // should update its |has_created_frame_sink_| and |is_root_| properties.
  it->second->SetHasCreatedFrameSink(true);
  it->second->SetRoot(is_root);
}

void DOMAgentViz::OnDestroyedCompositorFrameSink(
    const viz::FrameSinkId& frame_sink_id) {
  auto it = frame_sink_elements_.find(frame_sink_id);
  DCHECK(it != frame_sink_elements_.end());

  // Set FrameSinkElement to not connected to mark it as destroyed.
  it->second->SetHasCreatedFrameSink(false);
}

void DOMAgentViz::OnRegisteredFrameSinkHierarchy(
    const viz::FrameSinkId& parent_frame_sink_id,
    const viz::FrameSinkId& child_frame_sink_id) {
  // At this point these elements must be present in a tree.
  // We should detach a child from its current parent and attach to the new
  // parent.
  auto it_parent = frame_sink_elements_.find(parent_frame_sink_id);
  auto it_child = frame_sink_elements_.find(child_frame_sink_id);
  DCHECK(it_parent != frame_sink_elements_.end());
  DCHECK(it_child != frame_sink_elements_.end());

  FrameSinkElement* child = it_child->second.get();
  FrameSinkElement* new_parent = it_parent->second.get();

  // TODO: Add support for |child| to have multiple parents.
  child->Reparent(new_parent);
}

void DOMAgentViz::OnUnregisteredFrameSinkHierarchy(
    const viz::FrameSinkId& parent_frame_sink_id,
    const viz::FrameSinkId& child_frame_sink_id) {
  // At this point these elements must be present in a tree.
  // We should detach a child from its current parent and attach to the
  // RootElement since it wasn't destroyed yet.
  auto it_child = frame_sink_elements_.find(child_frame_sink_id);
  DCHECK(it_child != frame_sink_elements_.end());

  FrameSinkElement* child = it_child->second.get();

  // TODO: Add support for |child| to have multiple parents: only adds |child|
  // to RootElement if all parents of |child| are unregistered.
  child->Reparent(element_root());
}

SurfaceElement* DOMAgentViz::GetRootSurfaceElement() {
  auto it = surface_elements_.find(surface_manager_->GetRootSurfaceId());
  DCHECK(it != surface_elements_.end());
  return it->second.get();
}

std::unique_ptr<protocol::DOM::Node> DOMAgentViz::BuildTreeForFrameSink(
    UIElement* parent_element,
    const viz::FrameSinkId& parent_id) {
  auto children = std::make_unique<protocol::Array<protocol::DOM::Node>>();

  // Once the FrameSinkElement is created it calls this function to build its
  // subtree. We iterate through |parent_element|'s children and
  // recursively build the subtree for them.
  for (auto& child_id : frame_sink_manager_->GetChildrenByParent(parent_id)) {
    bool has_created_frame_sink =
        !!frame_sink_manager_->GetFrameSinkForId(child_id);

    FrameSinkElement* child_element = CreateFrameSinkElement(
        child_id, parent_element, /*is_root=*/false, has_created_frame_sink);

    children->emplace_back(BuildTreeForFrameSink(child_element, child_id));
    child_element->AddToParentSorted(parent_element);
  }

  return BuildNode("FrameSink",
                   std::make_unique<std::vector<std::string>>(
                       parent_element->GetAttributes()),
                   std::move(children), parent_element->node_id());
}

std::unique_ptr<protocol::DOM::Node> DOMAgentViz::BuildTreeForSurface(
    UIElement* parent_element,
    const viz::SurfaceId& parent_id) {
  auto children = std::make_unique<protocol::Array<protocol::DOM::Node>>();

  // Once the SurfaceElement is created it calls this function to build its
  // subtree. We iterate through |parent_element|'s children and
  // recursively build the subtree for them.
  for (auto& child_id :
       surface_manager_->GetSurfacesReferencedByParent(parent_id)) {
    SurfaceElement* child_element;
    // If the child element exists already, move it here.
    auto it_child = surface_elements_.find(child_id);
    if (it_child != surface_elements_.end()) {
      child_element = it_child->second.get();
      child_element->Reparent(parent_element);
    } else {
      child_element = CreateSurfaceElement(child_id, parent_element);
      child_element->AddToParentSorted(parent_element);
    }

    children->emplace_back(BuildTreeForSurface(child_element, child_id));
  }

  return BuildNode("Surface",
                   std::make_unique<std::vector<std::string>>(
                       parent_element->GetAttributes()),
                   std::move(children), parent_element->node_id());
}

protocol::Response DOMAgentViz::enable() {
  frame_sink_manager_->AddObserver(this);
  surface_manager_->AddObserver(this);
  return protocol::Response::OK();
}

protocol::Response DOMAgentViz::disable() {
  frame_sink_manager_->RemoveObserver(this);
  surface_manager_->RemoveObserver(this);
  Clear();
  return DOMAgent::disable();
}

std::vector<UIElement*> DOMAgentViz::CreateChildrenForRoot() {
  std::vector<UIElement*> children;

  // All of the FrameSinkElements and SurfaceElements are owned here, so make
  // sure the root element doesn't delete our pointers.
  element_root()->set_owns_children(false);

  // Find all elements that are not part of any hierarchy. This will be
  // FrameSinks that are either root, or detached.
  std::vector<viz::FrameSinkId> registered_frame_sinks =
      frame_sink_manager_->GetRegisteredFrameSinkIds();
  base::flat_set<viz::FrameSinkId> detached_frame_sinks(registered_frame_sinks);

  for (auto& frame_sink_id : registered_frame_sinks) {
    for (auto& child_id :
         frame_sink_manager_->GetChildrenByParent(frame_sink_id)) {
      detached_frame_sinks.erase(child_id);
    }
  }

  // Add created RootFrameSinks and detached FrameSinks.
  for (auto& frame_sink_id : detached_frame_sinks) {
    const viz::CompositorFrameSinkSupport* support =
        frame_sink_manager_->GetFrameSinkForId(frame_sink_id);
    bool is_root = support && support->is_root();
    bool has_created_frame_sink = !!support;
    children.push_back(CreateFrameSinkElement(frame_sink_id, element_root(),
                                              is_root, has_created_frame_sink));
  }

  // Add the root surface.
  children.push_back(CreateSurfaceElement(surface_manager_->GetRootSurfaceId(),
                                          element_root()));

  return children;
}

std::unique_ptr<protocol::DOM::Node> DOMAgentViz::BuildTreeForUIElement(
    UIElement* ui_element) {
  if (ui_element->type() == UIElementType::FRAMESINK) {
    return BuildTreeForFrameSink(ui_element,
                                 FrameSinkElement::From(ui_element));
  } else if (ui_element->type() == UIElementType::SURFACE) {
    return BuildTreeForSurface(ui_element, SurfaceElement::From(ui_element));
  }
  return nullptr;
}

void DOMAgentViz::Clear() {
  frame_sink_elements_.clear();
  surface_elements_.clear();
}

void DOMAgentViz::DestroyElementAndRemoveSubtree(UIElement* element) {
  // We may come across the case where we've got the event to delete the
  // FrameSink or Surface, but we haven't got events to delete its children. We
  // should detach all its children and attach them to either RootElement or the
  // root surface, and then delete the node we were asked for.
  UIElement* new_parent =
      (element->type() == SURFACE ? GetRootSurfaceElement() : element_root());
  // Make a copy of the list of children, so that it isn't affected when
  // elements are moved.
  std::vector<UIElement*> children(element->children());
  for (auto* child : children)
    VizElement::AsVizElement(child)->Reparent(new_parent);

  element->parent()->RemoveChild(element);
  DestroyElement(element);
}

void DOMAgentViz::DestroyElement(UIElement* element) {
  if (element->type() == UIElementType::FRAMESINK) {
    frame_sink_elements_.erase(FrameSinkElement::From(element));
  } else if (element->type() == UIElementType::SURFACE) {
    surface_elements_.erase(SurfaceElement::From(element));
  } else {
    NOTREACHED();
  }
}

FrameSinkElement* DOMAgentViz::CreateFrameSinkElement(
    const viz::FrameSinkId& frame_sink_id,
    UIElement* parent,
    bool is_root,
    bool is_client_connected) {
  DCHECK(!base::Contains(frame_sink_elements_, frame_sink_id));
  frame_sink_elements_[frame_sink_id] = std::make_unique<FrameSinkElement>(
      frame_sink_id, frame_sink_manager_, this, parent, is_root,
      is_client_connected);
  return frame_sink_elements_[frame_sink_id].get();
}

SurfaceElement* DOMAgentViz::CreateSurfaceElement(
    const viz::SurfaceId& surface_id,
    UIElement* parent) {
  DCHECK(!base::Contains(surface_elements_, surface_id));
  surface_elements_[surface_id] = std::make_unique<SurfaceElement>(
      surface_id, frame_sink_manager_, this, parent);
  return surface_elements_[surface_id].get();
}

}  // namespace ui_devtools
