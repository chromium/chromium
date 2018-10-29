// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_EXTENSIONS_AUTOMATION_AX_TREE_WRAPPER_H_
#define CHROMECAST_RENDERER_EXTENSIONS_AUTOMATION_AX_TREE_WRAPPER_H_

#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/ax_tree.h"

struct ExtensionMsg_AccessibilityEventBundleParams;

namespace extensions {
namespace cast {

class AutomationInternalCustomBindings;

// A class that wraps one AXTree and all of the additional state
// and helper methods needed to use it for the automation API.
class AutomationAXTreeWrapper : public ui::AXEventGenerator {
 public:
  AutomationAXTreeWrapper(ui::AXTreeID tree_id,
                          AutomationInternalCustomBindings* owner);
  ~AutomationAXTreeWrapper() override;

  ui::AXTreeID tree_id() const { return tree_id_; }
  ui::AXTree* tree() { return &tree_; }
  AutomationInternalCustomBindings* owner() { return owner_; }

  // The host node ID is the node ID of the parent node in the parent tree.
  // For example, the host node ID of a web area of a child frame is the
  // ID of the <iframe> element in its parent frame.
  int32_t host_node_id() const { return host_node_id_; }
  void set_host_node_id(int32_t id) { host_node_id_ = id; }

  // Called by AutomationInternalCustomBindings::OnAccessibilityEvents on
  // the AutomationAXTreeWrapper instance for the correct tree corresponding
  // to this event. Unserializes the tree update and calls back to
  // AutomationInternalCustomBindings to fire any automation events needed.
  bool OnAccessibilityEvents(
      const ExtensionMsg_AccessibilityEventBundleParams& events,
      bool is_active_profile);

 private:
  // AXEventGenerator overrides.
  void OnNodeDataWillChange(ui::AXTree* tree,
                            const ui::AXNodeData& old_node_data,
                            const ui::AXNodeData& new_node_data) override;
  void OnNodeWillBeDeleted(ui::AXTree* tree, ui::AXNode* node) override;
  void OnAtomicUpdateFinished(ui::AXTree* tree,
                              bool root_changed,
                              const std::vector<Change>& changes) override;

  // Given an event, return true if the event is handled by
  // AXEventGenerator, and false if it's not. Temporary, this will be
  // removed with the AXEventGenerator refactoring is complete.
  bool IsEventTypeHandledByAXEventGenerator(api::automation::EventType) const;

  ui::AXTreeID tree_id_;
  int32_t host_node_id_;
  ui::AXTree tree_;
  AutomationInternalCustomBindings* owner_;
  std::vector<int> deleted_node_ids_;
  std::vector<int> text_changed_node_ids_;

  DISALLOW_COPY_AND_ASSIGN(AutomationAXTreeWrapper);
};

}  // namespace cast
}  // namespace extensions

#endif  // CHROMECAST_RENDERER_EXTENSIONS_AUTOMATION_AX_TREE_WRAPPER_H_
