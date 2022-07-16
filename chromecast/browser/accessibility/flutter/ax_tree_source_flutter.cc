// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/accessibility/flutter/ax_tree_source_flutter.h"

#include <stack>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chromecast/browser/accessibility/accessibility_manager.h"
#include "chromecast/browser/accessibility/flutter/flutter_semantics_node_wrapper.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_web_contents_observer.h"
#include "chromecast/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/tts_utterance.h"
#include "extensions/browser/api/automation_internal/automation_event_router.h"
#include "extensions/browser/api/automation_internal/automation_event_router_interface.h"
#include "ui/accessibility/aura/aura_window_properties.h"
#include "ui/aura/window.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {
ax::mojom::Event ToAXEvent(
    gallium::castos::OnAccessibilityEventRequest_EventType flutter_event_type) {
  switch (flutter_event_type) {
    case gallium::castos::OnAccessibilityEventRequest_EventType_FOCUSED:
      return ax::mojom::Event::kFocus;
    case gallium::castos::OnAccessibilityEventRequest_EventType_CLICKED:
    case gallium::castos::OnAccessibilityEventRequest_EventType_LONG_CLICKED:
      return ax::mojom::Event::kClicked;
    case gallium::castos::OnAccessibilityEventRequest_EventType_TEXT_CHANGED:
      return ax::mojom::Event::kTextChanged;
    case gallium::castos::
        OnAccessibilityEventRequest_EventType_TEXT_SELECTION_CHANGED:
      return ax::mojom::Event::kTextSelectionChanged;
    case gallium::castos::OnAccessibilityEventRequest_EventType_HOVER_ENTER:
      return ax::mojom::Event::kHover;
    case gallium::castos::OnAccessibilityEventRequest_EventType_SCROLLED:
      return ax::mojom::Event::kScrollPositionChanged;
    case gallium::castos::OnAccessibilityEventRequest_EventType_CONTENT_CHANGED:
      return ax::mojom::Event::kChildrenChanged;
    case gallium::castos::
        OnAccessibilityEventRequest_EventType_WINDOW_STATE_CHANGED:
      return ax::mojom::Event::kLayoutComplete;
    default:
      return ax::mojom::Event::kNone;
  }
}
}  // namespace

namespace chromecast {
namespace accessibility {

AXTreeSourceFlutter::AXTreeWebContentsObserver::AXTreeWebContentsObserver(
    content::WebContents* web_contents,
    AXTreeSourceFlutter* ax_tree_source)
    : WebContentsObserver(web_contents), ax_tree_source_(ax_tree_source) {}

void AXTreeSourceFlutter::AXTreeWebContentsObserver::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  ax_tree_source_->UpdateTree();
}

void AXTreeSourceFlutter::AXTreeWebContentsObserver::
    AXTreeIDForMainFrameHasChanged() {
  ax_tree_source_->UpdateTree();
}

constexpr int kInvalidId = -1;

AXTreeSourceFlutter::AXTreeSourceFlutter(
    Delegate* delegate,
    content::BrowserContext* browser_context,
    extensions::AutomationEventRouterInterface* event_router)
    : current_tree_serializer_(std::make_unique<AXTreeFlutterSerializer>(this)),
      root_id_(kInvalidId),
      window_id_(kInvalidId),
      focused_id_(kInvalidId),
      delegate_(delegate),
      browser_context_(browser_context),
      event_router_(event_router
                        ? event_router
                        : extensions::AutomationEventRouter::GetInstance()),
      cast_web_contents_(nullptr),
      accessibility_enabled_(false) {
  DCHECK(delegate_);
}

AXTreeSourceFlutter::~AXTreeSourceFlutter() {
  Reset();
}

void AXTreeSourceFlutter::NotifyAccessibilityEvent(
    const gallium::castos::OnAccessibilityEventRequest* event_data) {
  DCHECK(event_data);

  if (event_data->node_data_size() > 0) {
    // Remember most recent tree in case we need to update parents
    // of child trees with new ax tree ids (i.e. due to embedded webview
    // navigation)
    last_event_data_ = *event_data;
  }

  // First find out if we know what to do with this event type from flutter.
  if (event_data->event_type() ==
      gallium::castos::OnAccessibilityEventRequest_EventType_ANNOUNCEMENT) {
    if (!event_data->has_text())
      return;

    SubmitTTS(event_data->text());
    return;
  }

  ax::mojom::Event translated_event = ToAXEvent(event_data->event_type());
  if (translated_event == ax::mojom::Event::kNone) {
    LOG(INFO) << "Ignoring unknown flutter ax event "
              << event_data->event_type() << ". No mapping available.";
    return;
  }

  // b/150992421 - We sometimes get nodes that have been reparented.
  // Any node that is reparented must first be deleted from its old
  // parent before appearing under a new one. The tree serializer
  // should be handling this case for us but isn't so the tree
  // update fails. As a workaround until this is fixed upstream, we
  // will identify these nodes ourselves and provide a separate update
  // that will delete them prior to the full update.
  reparented_children_.clear();
  std::vector<int32_t> parents_with_deleted_children;
  for (int i = 0; i < event_data->node_data_size(); ++i) {
    const SemanticsNode& node = event_data->node_data(i);
    for (int j = 0; j < node.child_node_ids_size(); ++j) {
      int child_id = node.child_node_ids(j);
      auto it = parent_map_.find(child_id);
      if (it != parent_map_.end()) {
        if (node.node_id() != it->second) {
          // Remember this child and who its parent was.
          reparented_children_.push_back(child_id);
          parents_with_deleted_children.push_back(it->second);
        }
      }
    }
  }

  tree_map_.clear();
  cached_computed_bounds_.clear();
  if (event_data->node_data_size() > 0) {
    // Unless there are new nodes, don't clear previous parent map so we
    // can detect reparenting above.
    parent_map_.clear();
  }

  window_id_ = event_data->window_id();

  // The following loops perform caching to prepare for AXTreeSerializer.
  // First, we need to cache parent links, which are implied by a node's child
  // ids. Next, we cache the nodes by id. During this process, we can detect
  // the root node based upon the parent links we cached above. Finally, we
  // cache each node's computed bounds, based on its descendants.
  std::map<int32_t, int32_t> all_parent_map;
  std::map<int32_t, std::vector<int32_t>> all_children_map;
  for (int i = 0; i < event_data->node_data_size(); ++i) {
    const SemanticsNode& node = event_data->node_data(i);
    for (int j = 0; j < node.child_node_ids_size(); ++j) {
      all_children_map[node.node_id()].push_back(node.child_node_ids(j));
      all_parent_map[node.child_node_ids(j)] = node.node_id();
    }
  }

  // Now copy just the relevant subtree containing the source_id into the
  // |parent_map_|.
  root_id_ = event_data->source_id();
  // Walk up to the root from the source_id.
  for (auto it = all_parent_map.find(root_id_); it != all_parent_map.end();
       it = all_parent_map.find(root_id_)) {
    root_id_ = it->second;
  }

  // Walk back down through children map to populate parent_map_.
  std::stack<int32_t> stack;
  stack.push(root_id_);
  while (!stack.empty()) {
    int32_t parent = stack.top();
    stack.pop();
    for (int32_t child_id : all_children_map[parent]) {
      parent_map_[child_id] = parent;
      stack.push(child_id);
    }
  }

  std::vector<std::string> new_child_trees;
  for (int i = 0; i < event_data->node_data_size(); ++i) {
    int32_t id = event_data->node_data(i).node_id();
    // Only map nodes in the parent_map and the root.
    // This avoids adding other subtrees that are not interesting.
    if (parent_map_.find(id) == parent_map_.end() && id != root_id_)
      continue;
    const SemanticsNode& node = event_data->node_data(i);
    tree_map_[id] = std::make_unique<FlutterSemanticsNodeWrapper>(this, &node);
    if (tree_map_[id]->IsFocused()) {
      focused_id_ = id;
    }
    // Place focus on the node that holds a child tree. When the
    // child tree is removed, focus will be placed back on the root.
    if (node.has_plugin_id()) {
      std::string ax_tree_id = node.plugin_id();
      new_child_trees.push_back(ax_tree_id);

      if (std::find(child_trees_.begin(), child_trees_.end(), ax_tree_id) ==
          child_trees_.end()) {
        if (ax_tree_id.rfind("T:", 0) == 0) {
          int web_contents_id;
          base::StringToInt(ax_tree_id.substr(2), &web_contents_id);
          std::vector<CastWebContents*> all_contents =
              CastWebContents::GetAll();
          for (CastWebContents* contents : all_contents) {
            if (contents->id() == web_contents_id &&
                child_tree_observers_.find(web_contents_id) ==
                    child_tree_observers_.end()) {
              child_tree_observers_[contents->id()] = std::make_unique<
                  AXTreeSourceFlutter::AXTreeWebContentsObserver>(
                  contents->web_contents(), this);
              CastWebContentsObserver::Observe(contents);
              cast_web_contents_ = contents;
              break;
            }
          }
        }
      }

      focused_id_ = node.node_id();
    }
  }

  // Do we need to put focus somewhere after a child tree
  // has been removed?
  bool need_focus_clear = false;
  for (std::string id : child_trees_) {
    // Is this old child tree still known?
    if (std::find(new_child_trees.begin(), new_child_trees.end(), id) ==
        new_child_trees.end()) {
      // No, clear focus.
      need_focus_clear = true;
      focused_id_ = root_id_;
      break;
    }
  }
  child_trees_ = new_child_trees;

  // Assuming |nodeData| is in pre-order, compute cached bounds in post-order to
  // avoid an O(n^2) amount of work as the computed bounds uses descendant
  // bounds.
  for (int i = 0; i < event_data->node_data_size(); ++i) {
    int32_t id = event_data->node_data(i).node_id();
    if (parent_map_.find(id) == parent_map_.end() && id != root_id_)
      continue;
    cached_computed_bounds_[id] = ComputeEnclosingBounds(tree_map_[id].get());
  }

  // If focus was not set from above, set it now on the root node.
  if (focused_id_ < 0 && root_id_ >= 0) {
    focused_id_ = root_id_;
  }

  std::vector<ui::AXEvent> events;
  ui::AXEvent event;
  event.event_type = translated_event;
  event.id = event_data->source_id();
  events.push_back(std::move(event));

  if (event_data->event_type() ==
      gallium::castos::OnAccessibilityEventRequest_EventType_CONTENT_CHANGED) {
    current_tree_serializer_->InvalidateSubtree(
        GetFromId(event_data->source_id()));
  }

  std::vector<ui::AXTreeUpdate> updates;
  if (event_data->event_type() !=
          gallium::castos::OnAccessibilityEventRequest_EventType_HOVER_ENTER &&
      event_data->event_type() !=
          gallium::castos::OnAccessibilityEventRequest_EventType_HOVER_EXIT) {
    // For every parent whose child has been moved, serialize an update.
    // This update will filter all the children that have moved.
    for (int32_t nid : parents_with_deleted_children) {
      ui::AXTreeUpdate update;
      current_tree_serializer_->SerializeChanges(GetFromId(nid), &update);
      updates.push_back(std::move(update));
    }

    // If there were any children that were reparented, invalidate the entire
    // tree so the new parents get the children.
    if (reparented_children_.size() > 0) {
      current_tree_serializer_->InvalidateSubtree(GetFromId(root_id_));
    }

    // Clear reparented children.
    reparented_children_.clear();

    // Handle routes added/removed from the tree.
    HandleRoutes(&events);

    ui::AXTreeUpdate update;
    current_tree_serializer_->SerializeChanges(
        GetFromId(event_data->source_id()), &update);
    updates.push_back(std::move(update));

    HandleLiveRegions(&events);

    // b/162311902: For nodes that have scroll extents, rapidly changing the
    // value will result in queueing up the values and speak out one by one.
    // Here we handle the tts natively.
    HandleNativeTTS();

    HandleVirtualKeyboardNodes();
  }

  // Need to refocus
  if (need_focus_clear) {
    ui::AXEvent focus_event;
    focus_event.event_type = ax::mojom::Event::kFocus;
    focus_event.id = focused_id_;
    focus_event.event_from = ax::mojom::EventFrom::kNone;
    focus_event.event_from_action = ax::mojom::Action::kNone;
    events.push_back(std::move(focus_event));
  }

  if (event_router_) {
    event_router_->DispatchAccessibilityEvents(ax_tree_id(), std::move(updates),
                                               gfx::Point(), std::move(events));
  }
}

void AXTreeSourceFlutter::NotifyActionResult(const ui::AXActionData& data,
                                             bool result) {
  if (!event_router_)
    return;

  event_router_->DispatchActionResult(data, result, browser_context_);
}

bool AXTreeSourceFlutter::GetTreeData(ui::AXTreeData* data) const {
  DCHECK(data);
  data->tree_id = ax_tree_id();
  if (focused_id_ >= 0) {
    data->focus_id = focused_id_;
  } else if (root_id_ >= 0) {
    data->focus_id = root_id_;
  }
  return true;
}

FlutterSemanticsNode* AXTreeSourceFlutter::GetRoot() const {
  return GetFromId(root_id_);
}

FlutterSemanticsNode* AXTreeSourceFlutter::GetFromId(int32_t id) const {
  auto it = tree_map_.find(id);
  if (it == tree_map_.end())
    return nullptr;
  return it->second.get();
}

int32_t AXTreeSourceFlutter::GetId(FlutterSemanticsNode* info_data) const {
  if (!info_data)
    return kInvalidId;
  return info_data->GetId();
}

void AXTreeSourceFlutter::GetChildren(
    FlutterSemanticsNode* info_data,
    std::vector<FlutterSemanticsNode*>* out_children) const {
  if (!info_data)
    return;

  info_data->GetChildren(out_children);
  if (out_children->empty())
    return;

  // Filter out any reparented children so the update doesn't see them.
  auto it = out_children->begin();
  while (it != out_children->end()) {
    if (std::find(reparented_children_.begin(), reparented_children_.end(),
                  (*it)->GetId()) != reparented_children_.end()) {
      it = out_children->erase(it);
    } else {
      ++it;
    }
  }
}

FlutterSemanticsNode* AXTreeSourceFlutter::GetParent(
    FlutterSemanticsNode* info_data) const {
  if (!info_data)
    return nullptr;
  auto it = parent_map_.find(info_data->GetId());
  if (it != parent_map_.end())
    return GetFromId(it->second);
  return nullptr;
}

bool AXTreeSourceFlutter::IsValid(FlutterSemanticsNode* info_data) const {
  return info_data;
}

bool AXTreeSourceFlutter::IsIgnored(FlutterSemanticsNode* info_data) const {
  return false;
}

bool AXTreeSourceFlutter::IsEqual(FlutterSemanticsNode* info_data1,
                                  FlutterSemanticsNode* info_data2) const {
  if (!info_data1 || !info_data2)
    return false;
  return info_data1->GetId() == info_data2->GetId();
}

FlutterSemanticsNode* AXTreeSourceFlutter::GetNull() const {
  return nullptr;
}

void AXTreeSourceFlutter::SerializeNode(FlutterSemanticsNode* info_data,
                                        ui::AXNodeData* out_data) const {
  if (!info_data)
    return;

  int32_t id = info_data->GetId();
  out_data->id = id;
  if (id == root_id_) {
    out_data->role = ax::mojom::Role::kRootWebArea;
  } else {
    info_data->PopulateAXRole(out_data);
  }

  info_data->Serialize(out_data);
}

gfx::Rect AXTreeSourceFlutter::ComputeEnclosingBounds(
    FlutterSemanticsNode* info_data) const {
  gfx::Rect computed_bounds;
  // Exit early if the node or window is invisible.
  if (!info_data->IsVisibleToUser())
    return computed_bounds;

  ComputeEnclosingBoundsInternal(info_data, &computed_bounds);
  return computed_bounds;
}

void AXTreeSourceFlutter::ComputeEnclosingBoundsInternal(
    FlutterSemanticsNode* info_data,
    gfx::Rect* computed_bounds) const {
  auto cached_bounds = cached_computed_bounds_.find(info_data->GetId());
  if (cached_bounds != cached_computed_bounds_.end()) {
    computed_bounds->Union(cached_bounds->second);
    return;
  }

  if (!info_data->IsVisibleToUser())
    return;

  if (info_data->CanBeAccessibilityFocused()) {
    // Only consider nodes that can possibly be accessibility focused.
    computed_bounds->Union(info_data->GetBounds());
    return;
  }

  std::vector<FlutterSemanticsNode*> children;
  info_data->GetChildren(&children);
  if (children.empty())
    return;

  for (FlutterSemanticsNode* child : children)
    ComputeEnclosingBoundsInternal(child, computed_bounds);
}

void AXTreeSourceFlutter::PerformAction(const ui::AXActionData& data) {
  delegate_->OnAction(data);
}

void AXTreeSourceFlutter::Reset() {
  tree_map_.clear();
  parent_map_.clear();
  cached_computed_bounds_.clear();
  current_tree_serializer_ = std::make_unique<AXTreeFlutterSerializer>(this);
  root_id_ = kInvalidId;
  focused_id_ = kInvalidId;
  if (!event_router_)
    return;
  event_router_->DispatchTreeDestroyedEvent(ax_tree_id(), browser_context_);
}

void AXTreeSourceFlutter::HandleVirtualKeyboardNodes() {
  gfx::Rect bounds;
  for (const auto& it : tree_map_) {
    FlutterSemanticsNode* node_info = it.second.get();
    if (!node_info->IsKeyboardNode())
      continue;
    bounds.Union(node_info->GetBounds());
  }

  if (bounds != keyboard_bounds_) {
    keyboard_bounds_ = bounds;
    delegate_->OnVirtualKeyboardBoundsChange(keyboard_bounds_);
  }
}

void AXTreeSourceFlutter::HandleNativeTTS() {
  std::map<int32_t, std::string> new_native_tts_name_cache;

  // Cache current native tts name cache.
  for (const auto& it : tree_map_) {
    FlutterSemanticsNode* node_info = it.second.get();
    if (!node_info->IsRapidChangingSlider())
      continue;

    std::vector<std::string> names;
    if (node_info->HasLabelHint()) {
      names.push_back(node_info->GetLabelHint());
    }
    if (node_info->HasValue()) {
      names.push_back(node_info->GetValue());
    }

    new_native_tts_name_cache[node_info->GetId()] =
        base::JoinString(names, " ");
  }

  // Compare to the previous one, and send out TTS if needed.
  for (const auto& it : new_native_tts_name_cache) {
    auto prev_it = native_tts_name_cache_.find(it.first);
    if (prev_it != native_tts_name_cache_.end() &&
        prev_it->second != it.second) {
      // Send to TTS controller.
      SubmitTTS(it.second);
    }
  }

  std::swap(native_tts_name_cache_, new_native_tts_name_cache);
}

void AXTreeSourceFlutter::HandleLiveRegions(std::vector<ui::AXEvent>* events) {
  std::map<int32_t, std::string> new_live_region_map;

  // Cache current live region's name.
  for (const auto& it : tree_map_) {
    FlutterSemanticsNode* node_info = it.second.get();
    if (!node_info->IsLiveRegion())
      continue;

    std::stack<FlutterSemanticsNode*> stack;
    stack.push(node_info);
    while (!stack.empty()) {
      FlutterSemanticsNode* node = stack.top();
      stack.pop();
      DCHECK(node);

      ui::AXNodeData data;
      SerializeNode(node, &data);
      std::string name;
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name);
      new_live_region_map[node->GetId()] = name;

      std::vector<FlutterSemanticsNode*> children;
      node->GetChildren(&children);
      for (FlutterSemanticsNode* child : children)
        stack.push(child);
    }
  }

  // Compare to the previous one, and add an event if needed.
  for (const auto& it : new_live_region_map) {
    auto prev_it = live_region_name_cache_.find(it.first);
    if (prev_it == live_region_name_cache_.end())
      continue;

    if (prev_it->second != it.second) {
      events->emplace_back();
      ui::AXEvent& event = events->back();
      event.event_type = ax::mojom::Event::kLiveRegionChanged;
      event.id = it.first;
    }
  }

  std::swap(live_region_name_cache_, new_live_region_map);
}

// Handle created/deleted nodes with scopes routes flag set.
void AXTreeSourceFlutter::HandleRoutes(std::vector<ui::AXEvent>* events) {
  bool focused_new = false;
  for (const auto& it : tree_map_) {
    FlutterSemanticsNode* node = it.second.get();
    if (!node->HasScopesRoute())
      continue;

    // Do we know about this node already? If so, skip.
    if (std::find(scopes_route_cache_.begin(), scopes_route_cache_.end(),
                  node->GetId()) != scopes_route_cache_.end()) {
      continue;
    }

    // Find a node in the sub-tree with names route flag set.
    FlutterSemanticsNode* sub_node = FindRoutesNode(node);
    if (sub_node) {
      // Only register the scopes route node in our cache
      // if a names route is found.
      scopes_route_cache_.push_back(node->GetId());

      ui::AXNodeData data;
      SerializeNode(sub_node, &data);
      std::string name;
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name);
      if (name.length() > 0) {
        focused_new = true;

        // Focus the node.
        focused_id_ = sub_node->GetId();
        events->emplace_back();
        ui::AXEvent& focus_event = events->back();
        focus_event.event_type = ax::mojom::Event::kFocus;
        focus_event.id = focused_id_;
        focus_event.event_from = ax::mojom::EventFrom::kNone;
        focus_event.event_from_action = ax::mojom::Action::kNone;
      }
    }
  }

  // Clean up the cache for those nodes that should not be in the cache anymore.
  bool need_refocus = false;
  for (std::vector<int32_t>::iterator it = scopes_route_cache_.begin();
       it != scopes_route_cache_.end();) {
    int32_t id = *it;
    FlutterSemanticsNode* scopes_route_node = GetFromId(id);
    if (scopes_route_node == nullptr || !scopes_route_node->HasScopesRoute() ||
        FindRoutesNode(scopes_route_node) == nullptr) {
      // This was removed in the latest tree update or there are no more
      // RoutesNode child.
      it = scopes_route_cache_.erase(it++);
      need_refocus = true;
    } else {
      it++;
    }
  }

  // After a deletion, use the last scopes route node to refocus on a child
  // with names route set (unless we already focused on a new node from above).
  if (need_refocus && !focused_new) {
    // Select the last in-depth node with scopesRoute for refocus
    FlutterSemanticsNode* refocused_routes_node = nullptr;
    if (scopes_route_cache_.size() > 0)
      refocused_routes_node =
          FindRoutesNode(GetFromId(scopes_route_cache_.back()));
    if (refocused_routes_node) {
      focused_id_ = refocused_routes_node->GetId();
    } else {
      focused_id_ = FindFirstFocusableNodeId();
    }
    events->emplace_back();
    ui::AXEvent& focus_event = events->back();
    focus_event.event_type = ax::mojom::Event::kFocus;
    focus_event.id = focused_id_;
    focus_event.event_from = ax::mojom::EventFrom::kNone;
    focus_event.event_from_action = ax::mojom::Action::kNone;
  }
}

// Perform depth first search for a subtree node under 'parent'
// with names route flag set.
FlutterSemanticsNode* AXTreeSourceFlutter::FindRoutesNode(
    FlutterSemanticsNode* parent) {
  if (parent == nullptr)
    return nullptr;

  std::stack<FlutterSemanticsNode*> stack;
  stack.push(parent);
  while (!stack.empty()) {
    FlutterSemanticsNode* node = stack.top();
    stack.pop();
    DCHECK(node);

    if (node->HasNamesRoute()) {
      return node;
    }

    std::vector<FlutterSemanticsNode*> children;
    node->GetChildren(&children);
    for (FlutterSemanticsNode* child : children)
      stack.push(child);
  }
  return nullptr;
}

// Find the first focusable node.
int32_t AXTreeSourceFlutter::FindFirstFocusableNodeId() {
  std::stack<FlutterSemanticsNode*> stack;
  stack.push(GetFromId(root_id_));
  while (!stack.empty()) {
    FlutterSemanticsNode* node = stack.top();
    stack.pop();
    DCHECK(node);

    if (node->CanBeAccessibilityFocused()) {
      return node->GetId();
    }

    std::vector<FlutterSemanticsNode*> children;
    node->GetChildren(&children);
    for (FlutterSemanticsNode* child : children)
      stack.push(child);
  }

  // Fallback to root if none found.
  return root_id_;
}

void AXTreeSourceFlutter::SubmitTTS(const std::string& text) {
  if (!accessibility_enabled_)
    return;

  std::unique_ptr<content::TtsUtterance> utterance =
      content::TtsUtterance::Create(browser_context_);

  utterance->SetText(text);

  auto* tts_controller = content::TtsController::GetInstance();
  tts_controller->Stop();
  tts_controller->SpeakOrEnqueue(std::move(utterance));
}

void AXTreeSourceFlutter::UpdateTree() {
  // Update the tree with last known flutter nodes.
  // TODO: A more efficient update would be to isolate just the parent node
  // whose child has changed. Consider giving the node to the observer
  // class on creation and passing through here.
  NotifyAccessibilityEvent(&last_event_data_);
}

void AXTreeSourceFlutter::PageStopped(PageState page_state, int error_code) {
  // Webview is gone. Stop observing.
  CastWebContentsObserver::Observe(nullptr);
  child_tree_observers_.erase(cast_web_contents_->id());
  cast_web_contents_ = nullptr;
}

void AXTreeSourceFlutter::SetAccessibilityEnabled(bool value) {
  accessibility_enabled_ = value;
}

}  // namespace accessibility
}  // namespace chromecast
