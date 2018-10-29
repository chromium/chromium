// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/dom_agent.h"

#include <memory>
#include <string>
#include <utility>

#include "components/ui_devtools/devtools_server.h"
#include "components/ui_devtools/root_element.h"
#include "components/ui_devtools/ui_element.h"

namespace ui_devtools {

using ui_devtools::protocol::Array;
using ui_devtools::protocol::DOM::Node;
using ui_devtools::protocol::Response;

DOMAgent::DOMAgent() {}

DOMAgent::~DOMAgent() {
  Reset();
}

Response DOMAgent::disable() {
  Reset();
  return Response::OK();
}

Response DOMAgent::getDocument(std::unique_ptr<Node>* out_root) {
  *out_root = BuildInitialTree();
  return Response::OK();
}

Response DOMAgent::pushNodesByBackendIdsToFrontend(
    std::unique_ptr<protocol::Array<int>> backend_node_ids,
    std::unique_ptr<protocol::Array<int>>* result) {
  *result = protocol::Array<int>::create();
  for (size_t index = 0; index < backend_node_ids->length(); ++index)
    (*result)->addItem(backend_node_ids->get(index));
  return Response::OK();
}

void DOMAgent::OnUIElementAdded(UIElement* parent, UIElement* child) {
  // When parent is null, only need to update |node_id_to_ui_element_|.
  if (!parent) {
    node_id_to_ui_element_[child->node_id()] = child;
    return;
  }

  DCHECK(node_id_to_ui_element_.count(parent->node_id()));

  auto* current_parent = parent;
  while (current_parent) {
    if (current_parent->is_updating()) {
      // One of the parents is updating, so no need to update here.
      return;
    }
    current_parent = current_parent->parent();
  }

  child->set_is_updating(true);

  const auto& children = parent->children();
  auto iter = std::find(children.begin(), children.end(), child);
  int prev_node_id =
      (iter == children.end() - 1) ? 0 : (*std::next(iter))->node_id();
  frontend()->childNodeInserted(parent->node_id(), prev_node_id,
                                BuildTreeForUIElement(child));

  child->set_is_updating(false);
}

void DOMAgent::OnUIElementReordered(UIElement* parent, UIElement* child) {
  DCHECK(node_id_to_ui_element_.count(parent->node_id()));

  const auto& children = parent->children();
  auto iter = std::find(children.begin(), children.end(), child);
  int prev_node_id =
      (iter == children.begin()) ? 0 : (*std::prev(iter))->node_id();
  RemoveDomNode(child);
  frontend()->childNodeInserted(parent->node_id(), prev_node_id,
                                BuildDomNodeFromUIElement(child));
}

void DOMAgent::OnUIElementRemoved(UIElement* ui_element) {
  DCHECK(node_id_to_ui_element_.count(ui_element->node_id()));

  RemoveDomNode(ui_element);
  node_id_to_ui_element_.erase(ui_element->node_id());
}

void DOMAgent::OnUIElementBoundsChanged(UIElement* ui_element) {
  for (auto& observer : observers_)
    observer.OnElementBoundsChanged(ui_element);
}

void DOMAgent::AddObserver(DOMAgentObserver* observer) {
  observers_.AddObserver(observer);
}

void DOMAgent::RemoveObserver(DOMAgentObserver* observer) {
  observers_.RemoveObserver(observer);
}

UIElement* DOMAgent::GetElementFromNodeId(int node_id) const {
  auto it = node_id_to_ui_element_.find(node_id);
  if (it != node_id_to_ui_element_.end())
    return it->second;
  return nullptr;
}

int DOMAgent::GetParentIdOfNodeId(int node_id) const {
  DCHECK(node_id_to_ui_element_.count(node_id));
  const UIElement* element = node_id_to_ui_element_.at(node_id);
  if (element->parent() && element->parent() != element_root_.get())
    return element->parent()->node_id();
  return 0;
}

// TODO(mhashmi): Make ids reusable

std::unique_ptr<Node> DOMAgent::BuildNode(
    const std::string& name,
    std::unique_ptr<Array<std::string>> attributes,
    std::unique_ptr<Array<Node>> children,
    int node_ids) {
  constexpr int kDomElementNodeType = 1;
  std::unique_ptr<Node> node = Node::create()
                                   .setNodeId(node_ids)
                                   .setBackendNodeId(node_ids)
                                   .setNodeName(name)
                                   .setNodeType(kDomElementNodeType)
                                   .setAttributes(std::move(attributes))
                                   .build();
  node->setChildNodeCount(static_cast<int>(children->length()));
  node->setChildren(std::move(children));
  return node;
}

std::unique_ptr<Node> DOMAgent::BuildDomNodeFromUIElement(UIElement* root) {
  std::unique_ptr<Array<Node>> children = Array<Node>::create();
  for (auto* it : root->children())
    children->addItem(BuildDomNodeFromUIElement(it));

  return BuildNode(root->GetTypeName(), root->GetAttributes(),
                   std::move(children), root->node_id());
}

std::unique_ptr<Node> DOMAgent::BuildInitialTree() {
  std::unique_ptr<Array<Node>> children = Array<Node>::create();

  element_root_ = std::make_unique<RootElement>(this);
  element_root_->set_is_updating(true);

  for (auto* child : CreateChildrenForRoot()) {
    children->addItem(BuildTreeForUIElement(child));
    element_root_->AddChild(child);
  }
  std::unique_ptr<Node> root_node =
      BuildNode("root", nullptr, std::move(children), element_root_->node_id());
  element_root_->set_is_updating(false);
  return root_node;
}

void DOMAgent::OnElementBoundsChanged(UIElement* ui_element) {
  for (auto& observer : observers_)
    observer.OnElementBoundsChanged(ui_element);
}

void DOMAgent::RemoveDomNode(UIElement* ui_element) {
  for (auto* child_element : ui_element->children())
    RemoveDomNode(child_element);
  frontend()->childNodeRemoved(ui_element->parent()->node_id(),
                               ui_element->node_id());
}

void DOMAgent::Reset() {
  element_root_.reset();
  node_id_to_ui_element_.clear();
  observers_.Clear();
}

}  // namespace ui_devtools
