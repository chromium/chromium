// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_EXTENSIONS_AUTOMATION_INTERNAL_CUSTOM_BINDINGS_H_
#define CHROMECAST_RENDERER_EXTENSIONS_AUTOMATION_INTERNAL_CUSTOM_BINDINGS_H_

#include <map>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chromecast/common/extensions_api/automation.h"
#include "chromecast/renderer/extensions/automation_ax_tree_wrapper.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "ipc/ipc_message.h"
#include "ui/accessibility/ax_tree.h"
#include "v8/include/v8.h"

struct ExtensionMsg_AccessibilityEventBundleParams;
struct ExtensionMsg_AccessibilityLocationChangeParams;

namespace ui {
struct AXEvent;
}

namespace extensions {
class ExtensionBindingsSystem;
}  // namespace extensions

namespace extensions {
namespace cast {

class AutomationInternalCustomBindings;
class AutomationMessageFilter;

struct TreeChangeObserver {
  int id;
  api::automation::TreeChangeObserverFilter filter;
};

// The native component of custom bindings for the chrome.automationInternal
// API.
class AutomationInternalCustomBindings : public ObjectBackedNativeHandler {
 public:
  AutomationInternalCustomBindings(ScriptContext* context,
                                   ExtensionBindingsSystem* bindings_system);
  ~AutomationInternalCustomBindings() override;

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

  void OnMessageReceived(const IPC::Message& message);

  AutomationAXTreeWrapper* GetAutomationAXTreeWrapperFromTreeID(
      ui::AXTreeID tree_id);

  // Given a tree (|in_out_tree_wrapper|) and a node, returns the parent.
  // If |node| is the root of its tree, the return value will be the host
  // node of the parent tree and |in_out_tree_wrapper| will be updated to
  // point to that parent tree.
  ui::AXNode* GetParent(ui::AXNode* node,
                        AutomationAXTreeWrapper** in_out_tree_wrapper);

  ScriptContext* context() const {
    return ObjectBackedNativeHandler::context();
  }

  float GetDeviceScaleFactor() const;

  void SendNodesRemovedEvent(ui::AXTree* tree, const std::vector<int>& ids);
  void SendTreeChangeEvent(api::automation::TreeChangeType change_type,
                           ui::AXTree* tree,
                           ui::AXNode* node);
  void SendAutomationEvent(ui::AXTreeID tree_id,
                           const gfx::Point& mouse_location,
                           ui::AXEvent& event,
                           api::automation::EventType event_type);

 private:
  // ObjectBackedNativeHandler overrides:
  void Invalidate() override;

  // Returns whether this extension has the "interact" permission set (either
  // explicitly or implicitly after manifest parsing).
  void IsInteractPermitted(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Returns an object with bindings that will be added to the
  // chrome.automation namespace.
  void GetSchemaAdditions(const v8::FunctionCallbackInfo<v8::Value>& args);

  // This is called by automation_internal_custom_bindings.js to indicate
  // that an API was called that needs access to accessibility trees. This
  // enables the MessageFilter that allows us to listen to accessibility
  // events forwarded to this process.
  void StartCachingAccessibilityTrees(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Called when an accessibility tree is destroyed and needs to be
  // removed from our cache.
  // Args: int ax_tree_id
  void DestroyAccessibilityTree(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  void AddTreeChangeObserver(const v8::FunctionCallbackInfo<v8::Value>& args);

  void RemoveTreeChangeObserver(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  void GetFocus(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Given an initial AutomationAXTreeWrapper, return the
  // AutomationAXTreeWrapper and node of the focused node within this tree or a
  // focused descendant tree.
  bool GetFocusInternal(AutomationAXTreeWrapper* top_tree,
                        AutomationAXTreeWrapper** out_tree,
                        ui::AXNode** out_node);

  void RouteTreeIDFunction(
      const std::string& name,
      void (*callback)(v8::Isolate* isolate,
                       v8::ReturnValue<v8::Value> result,
                       AutomationAXTreeWrapper* tree_wrapper));

  void RouteNodeIDFunction(
      const std::string& name,
      void (*callback)(v8::Isolate* isolate,
                       v8::ReturnValue<v8::Value> result,
                       AutomationAXTreeWrapper* tree_wrapper,
                       ui::AXNode* node));

  void RouteNodeIDPlusAttributeFunction(
      const std::string& name,
      void (*callback)(v8::Isolate* isolate,
                       v8::ReturnValue<v8::Value> result,
                       ui::AXTree* tree,
                       ui::AXNode* node,
                       const std::string& attribute_name));
  void RouteNodeIDPlusRangeFunction(
      const std::string& name,
      void (*callback)(v8::Isolate* isolate,
                       v8::ReturnValue<v8::Value> result,
                       AutomationAXTreeWrapper* tree_wrapper,
                       ui::AXNode* node,
                       int start,
                       int end));

  //
  // Access the cached accessibility trees and properties of their nodes.
  //

  // Args: int ax_tree_id, int node_id, Returns: int child_id.
  void GetChildIDAtIndex(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Args: int ax_tree_id, int node_id
  // Returns: JS object with a map from html attribute key to value.
  void GetHtmlAttributes(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Args: int ax_tree_id, int node_id
  // Returns: JS object with a string key for each state flag that's set.
  void GetState(const v8::FunctionCallbackInfo<v8::Value>& args);

  //
  // Helper functions.
  //

  // Handle accessibility events from the browser process.
  void OnAccessibilityEvents(
      const ExtensionMsg_AccessibilityEventBundleParams& events,
      bool is_active_profile);
  void OnAccessibilityLocationChange(
      const ExtensionMsg_AccessibilityLocationChangeParams& params);

  void UpdateOverallTreeChangeObserverFilter();

  void SendChildTreeIDEvent(ui::AXTree* tree, ui::AXNode* node);

  std::map<ui::AXTreeID, std::unique_ptr<AutomationAXTreeWrapper>>
      tree_id_to_tree_wrapper_map_;
  std::map<ui::AXTree*, AutomationAXTreeWrapper*> axtree_to_tree_wrapper_map_;
  scoped_refptr<AutomationMessageFilter> message_filter_;
  bool is_active_profile_;
  std::vector<TreeChangeObserver> tree_change_observers_;
  // A bit-map of api::automation::TreeChangeObserverFilter.
  int tree_change_observer_overall_filter_;
  ExtensionBindingsSystem* bindings_system_;
  bool should_ignore_context_;

  DISALLOW_COPY_AND_ASSIGN(AutomationInternalCustomBindings);
};

}  // namespace cast
}  // namespace extensions

#endif  // CHROMECAST_RENDERER_EXTENSIONS_AUTOMATION_INTERNAL_CUSTOM_BINDINGS_H_
