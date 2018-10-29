// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/viz_views/dom_agent_viz.h"

#include "base/stl_util.h"
#include "components/ui_devtools/root_element.h"
#include "components/ui_devtools/ui_element.h"
#include "components/ui_devtools/viz_views/frame_sink_element.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"

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
// in a tree we just change the properties (|is_registered_| or
// |is_client_connected_|). These events don't know anything about the hierarchy
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
// corresponding element. For example, if the element has |is_client_connected_|
// = true and |is_registered_| = true and we get a |OnDestroyedFrameSink| event
// we just set |is_client_connected_| = false, but don't remove it from a tree,
// because its FrameSinkId is still registered, so it's not completely dead. But
// when after that we get |OnInvalidatedFrameSinkId| we set |is_registered_| =
// false and since both these fields are false we can go ahead and remove the
// node from the tree. When we get OnUnregisteredHierarchy we assume the nodes
// are still present in a tree, so we do the same work as we did in registering
// case. Only here we move a subtree of parent rooted from child to the
// RootElement. Obviously, now the child will be in detached state.

using namespace ui_devtools::protocol;

DOMAgentViz::DOMAgentViz(viz::FrameSinkManagerImpl* frame_sink_manager)
    : frame_sink_manager_(frame_sink_manager) {}

DOMAgentViz::~DOMAgentViz() {
  Clear();
}

void DOMAgentViz::OnRegisteredFrameSinkId(
    const viz::FrameSinkId& frame_sink_id) {
  if (!base::ContainsKey(frame_sink_elements_, frame_sink_id)) {
    // If a FrameSink was just registered we don't know anything about
    // hierarchy. So we should attach it to the RootElement.
    element_root()->AddChild(
        new FrameSinkElement(frame_sink_id, frame_sink_manager_, this,
                             element_root(), /*is_root=*/false,
                             /*is_registered=*/true,
                             /*is_client_connected=*/false),
        element_root()->children().empty() ? nullptr
                                           : element_root()->children().back());
  }
}

void DOMAgentViz::OnInvalidatedFrameSinkId(
    const viz::FrameSinkId& frame_sink_id) {
  auto it = frame_sink_elements_.find(frame_sink_id);
  DCHECK(it != frame_sink_elements_.end());

  // Destroy the FrameSinkElement |element| after updating the frame-tree.
  std::unique_ptr<FrameSinkElement> element(it->second);
  element->SetRegistered(false);

  // A FrameSinkElement with |frame_sink_id| can only be invalidated after
  // being destroyed.
  DCHECK(!element->is_client_connected());
  RemoveFrameSinkSubtree(element.get());
  frame_sink_elements_.erase(frame_sink_id);
}

void DOMAgentViz::OnCreatedCompositorFrameSink(
    const viz::FrameSinkId& frame_sink_id,
    bool is_root) {
  auto frame_sink_element = frame_sink_elements_.find(frame_sink_id);
  DCHECK(frame_sink_element != frame_sink_elements_.end());
  // The corresponding element is already present in a tree, so we
  // should update its |is_client_connected_| and |is_root_| properties.
  frame_sink_element->second->SetClientConnected(true);
  frame_sink_element->second->SetRoot(is_root);
}

void DOMAgentViz::OnDestroyedCompositorFrameSink(
    const viz::FrameSinkId& frame_sink_id) {
  auto it = frame_sink_elements_.find(frame_sink_id);
  DCHECK(it != frame_sink_elements_.end());

  FrameSinkElement* element = it->second;
  // Set FrameSinkElement to not connected to make it as destroyed.
  element->SetClientConnected(false);
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

  FrameSinkElement* child = it_child->second;
  FrameSinkElement* new_parent = it_parent->second;

  // TODO: Add support for |child| to have multiple parents.
  if (child->parent())
    child->parent()->RemoveChild(child);

  new_parent->AddChild(child);
  child->set_parent(new_parent);
}

void DOMAgentViz::OnUnregisteredFrameSinkHierarchy(
    const viz::FrameSinkId& parent_frame_sink_id,
    const viz::FrameSinkId& child_frame_sink_id) {
  // At this point these elements must be present in a tree.
  // We should detach a child from its current parent and attach to the
  // RootElement since it wasn't destroyed yet.
  auto it_parent = frame_sink_elements_.find(parent_frame_sink_id);
  auto it_child = frame_sink_elements_.find(child_frame_sink_id);
  DCHECK(it_parent != frame_sink_elements_.end());
  DCHECK(it_child != frame_sink_elements_.end());

  FrameSinkElement* child = it_child->second;
  FrameSinkElement* parent = it_parent->second;

  parent->RemoveChild(child);
  // TODO: Add support for |child| to have multiple parents: only adds |child|
  // to RootElement if all parents of |child| are unregistered.
  child->set_parent(element_root());
}

std::unique_ptr<DOM::Node> DOMAgentViz::BuildTreeForFrameSink(
    FrameSinkElement* frame_sink_element,
    const viz::FrameSinkId& frame_sink_id) {
  frame_sink_elements_.emplace(frame_sink_id, frame_sink_element);
  std::unique_ptr<Array<DOM::Node>> children = Array<DOM::Node>::create();

  // Once the FrameSinkElement is created it calls this function to build its
  // subtree. So we iterate through |frame_sink_element|'s children and
  // recursively build the subtree for them.
  for (auto& child : frame_sink_manager_->GetChildrenByParent(frame_sink_id)) {
    bool is_registered = base::ContainsValue(
        frame_sink_manager_->GetRegisteredFrameSinkIds(), frame_sink_id);
    bool is_client_connected =
        is_registered &&
        base::ContainsValue(frame_sink_manager_->GetCreatedFrameSinkIds(),
                            frame_sink_id);

    FrameSinkElement* f_s_element = new FrameSinkElement(
        child, frame_sink_manager_, this, frame_sink_element,
        /*is_root=*/false, is_registered, is_client_connected);

    children->addItem(BuildTreeForFrameSink(f_s_element, child));
    frame_sink_element->AddChild(f_s_element);
  }
  std::unique_ptr<DOM::Node> node =
      BuildNode("FrameSink", frame_sink_element->GetAttributes(),
                std::move(children), frame_sink_element->node_id());
  return node;
}

protocol::Response DOMAgentViz::enable() {
  InitFrameSinkSets();
  frame_sink_manager_->AddObserver(this);
  return protocol::Response::OK();
}

protocol::Response DOMAgentViz::disable() {
  frame_sink_manager_->RemoveObserver(this);
  Clear();
  return DOMAgent::disable();
}

std::vector<UIElement*> DOMAgentViz::CreateChildrenForRoot() {
  std::vector<UIElement*> children;

  // Add created RootFrameSinks and detached FrameSinks.
  for (auto& frame_sink_id : frame_sink_manager_->GetRegisteredFrameSinkIds()) {
    if (base::ContainsValue(frame_sink_manager_->GetCreatedFrameSinkIds(),
                            frame_sink_id)) {
      const viz::CompositorFrameSinkSupport* support =
          frame_sink_manager_->GetFrameSinkForId(frame_sink_id);
      // Do nothing if it's a non-detached non-root FrameSink.
      if (support && !support->is_root() &&
          attached_frame_sinks_.find(frame_sink_id) !=
              attached_frame_sinks_.end()) {
        continue;
      }

      bool is_root = support && support->is_root();

      UIElement* frame_sink_element = new FrameSinkElement(
          frame_sink_id, frame_sink_manager_, this, element_root(), is_root,
          /*is_registered=*/true, /*is_client_connected=*/true);
      children.push_back(frame_sink_element);

      // Add registered but not created FrameSinks. If a FrameSinkId was
      // registered but not created we don't really know whether it's a root or
      // not. And we don't know any information about the hierarchy. Therefore
      // we process FrameSinks that are in the correct state first and only
      // after that we process registered but not created FrameSinks. We
      // consider them unembedded as well.
    } else {
      UIElement* frame_sink_element = new FrameSinkElement(
          frame_sink_id, frame_sink_manager_, this, element_root(),
          /*is_root=*/false, /*is_registered=*/true,
          /*is_client_connected=*/false);

      children.push_back(frame_sink_element);
    }
  }

  return children;
}

std::unique_ptr<DOM::Node> DOMAgentViz::BuildTreeForUIElement(
    UIElement* ui_element) {
  if (ui_element->type() == UIElementType::FRAMESINK) {
    viz::FrameSinkId frame_sink_id = FrameSinkElement::From(ui_element);

    bool is_registered = base::ContainsValue(
        frame_sink_manager_->GetRegisteredFrameSinkIds(), frame_sink_id);
    bool is_client_connected =
        is_registered &&
        base::ContainsValue(frame_sink_manager_->GetCreatedFrameSinkIds(),
                            frame_sink_id);
    FrameSinkElement* frame_sink_element = new FrameSinkElement(
        frame_sink_id, frame_sink_manager_, this, nullptr,
        /*is_root=*/false, is_registered, is_client_connected);

    return BuildTreeForFrameSink(frame_sink_element, frame_sink_id);
  }
  return nullptr;
}

void DOMAgentViz::Clear() {
  attached_frame_sinks_.clear();
  frame_sink_elements_.clear();
}

void DOMAgentViz::InitFrameSinkSets() {
  // Init the |attached_frame_sinks_| set. All RootFrameSinks and accessible
  // from roots are attached. All the others are detached.
  for (auto& frame_sink_id : frame_sink_manager_->GetRegisteredFrameSinkIds()) {
    if (base::ContainsValue(frame_sink_manager_->GetCreatedFrameSinkIds(),
                            frame_sink_id)) {
      const viz::CompositorFrameSinkSupport* support =
          frame_sink_manager_->GetFrameSinkForId(frame_sink_id);
      // Start only from roots.
      if (!support || !support->is_root())
        continue;

      SetAttachedFrameSink(frame_sink_id);
    }
  }
}

void DOMAgentViz::SetAttachedFrameSink(const viz::FrameSinkId& frame_sink_id) {
  attached_frame_sinks_.insert(frame_sink_id);
  for (auto& child : frame_sink_manager_->GetChildrenByParent(frame_sink_id))
    SetAttachedFrameSink(child);
}

void DOMAgentViz::RemoveFrameSinkSubtree(UIElement* root) {
  // We may come across the case where we've got the event to delete the
  // FrameSink, but we haven't got events to delete its children. We should
  // detach all its children and attach them to RootElement and then delete the
  // node we were asked for.
  std::vector<viz::FrameSinkId> children;
  for (auto* child : root->children())
    child->set_parent(element_root());

  if (root->parent())
    root->parent()->RemoveChild(root);
}

}  // namespace ui_devtools
