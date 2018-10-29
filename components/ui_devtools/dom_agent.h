// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_DOM_AGENT_H_
#define COMPONENTS_UI_DEVTOOLS_DOM_AGENT_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/observer_list.h"
#include "components/ui_devtools/DOM.h"
#include "components/ui_devtools/devtools_base_agent.h"
#include "components/ui_devtools/devtools_export.h"
#include "components/ui_devtools/ui_element_delegate.h"

namespace ui_devtools {

class UIElement;

class DOMAgentObserver {
 public:
  virtual void OnElementBoundsChanged(UIElement* ui_element) = 0;
};

class UI_DEVTOOLS_EXPORT DOMAgent
    : public UiDevToolsBaseAgent<protocol::DOM::Metainfo>,
      public UIElementDelegate {
 public:
  DOMAgent();
  ~DOMAgent() override;

  // DOM::Backend:
  protocol::Response disable() override;
  protocol::Response getDocument(
      std::unique_ptr<protocol::DOM::Node>* out_root) override;
  protocol::Response pushNodesByBackendIdsToFrontend(
      std::unique_ptr<protocol::Array<int>> backend_node_ids,
      std::unique_ptr<protocol::Array<int>>* result) override;

  // UIElementDelegate:
  void OnUIElementAdded(UIElement* parent, UIElement* child) override;
  void OnUIElementReordered(UIElement* parent, UIElement* child) override;
  void OnUIElementRemoved(UIElement* ui_element) override;
  void OnUIElementBoundsChanged(UIElement* ui_element) override;

  void AddObserver(DOMAgentObserver* observer);
  void RemoveObserver(DOMAgentObserver* observer);
  UIElement* GetElementFromNodeId(int node_id) const;
  UIElement* element_root() const { return element_root_.get(); }

  // Returns parent id of the element with id |node_id|. Returns 0 if parent
  // does not exist.
  int GetParentIdOfNodeId(int node_id) const;

 protected:
  std::unique_ptr<protocol::DOM::Node> BuildNode(
      const std::string& name,
      std::unique_ptr<protocol::Array<std::string>> attributes,
      std::unique_ptr<protocol::Array<protocol::DOM::Node>> children,
      int node_ids);
  std::unique_ptr<protocol::DOM::Node> BuildDomNodeFromUIElement(
      UIElement* root);

 private:
  // These are called on creating a DOM document.
  std::unique_ptr<protocol::DOM::Node> BuildInitialTree();

  // The caller takes ownership of the returned pointers.
  virtual std::vector<UIElement*> CreateChildrenForRoot() = 0;
  virtual std::unique_ptr<protocol::DOM::Node> BuildTreeForUIElement(
      UIElement* ui_element) = 0;

  void OnElementBoundsChanged(UIElement* ui_element);
  void RemoveDomNode(UIElement* ui_element);
  void Reset();

  std::unique_ptr<UIElement> element_root_;
  std::unordered_map<int, UIElement*> node_id_to_ui_element_;

  base::ObserverList<DOMAgentObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(DOMAgent);
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_DOM_AGENT_H_
