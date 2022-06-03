// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/accessibility/flutter/flutter_semantics_node_wrapper.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chromecast/browser/accessibility/flutter/ax_tree_source_flutter.h"
#include "chromecast/browser/cast_web_contents.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_action_handler_registry.h"

using gallium::castos::ActionProperties;
using gallium::castos::BooleanProperties;

namespace chromecast {
namespace accessibility {

FlutterSemanticsNodeWrapper::FlutterSemanticsNodeWrapper(
    ui::AXTreeSource<FlutterSemanticsNode*>* tree_source,
    const SemanticsNode* node)
    : tree_source_(tree_source), node_ptr_(node) {
  DCHECK(tree_source_);
  DCHECK(node_ptr_);
}

int32_t FlutterSemanticsNodeWrapper::GetId() const {
  return node_ptr_->node_id();
}

const gfx::Rect FlutterSemanticsNodeWrapper::GetBounds() const {
  if (node_ptr_->has_bounds_in_screen()) {
    return gfx::Rect(node_ptr_->bounds_in_screen().left(),
                     node_ptr_->bounds_in_screen().top(),
                     node_ptr_->bounds_in_screen().right() -
                         node_ptr_->bounds_in_screen().left(),
                     node_ptr_->bounds_in_screen().bottom() -
                         node_ptr_->bounds_in_screen().top());
  }
  return gfx::Rect(0, 0, 0, 0);
}

bool FlutterSemanticsNodeWrapper::IsVisibleToUser() const {
  if (node_ptr_->has_boolean_properties()) {
    const BooleanProperties& boolean_properties =
        node_ptr_->boolean_properties();
    return !boolean_properties.is_hidden();
  }
  return true;
}

bool FlutterSemanticsNodeWrapper::IsFocused() const {
  if (node_ptr_->has_boolean_properties()) {
    const BooleanProperties& boolean_properties =
        node_ptr_->boolean_properties();
    return boolean_properties.is_focused();
  }
  return false;
}

bool FlutterSemanticsNodeWrapper::IsLiveRegion() const {
  const BooleanProperties& boolean_properties = node_ptr_->boolean_properties();
  return boolean_properties.is_live_region();
}

bool FlutterSemanticsNodeWrapper::HasScopesRoute() const {
  const BooleanProperties& boolean_properties = node_ptr_->boolean_properties();
  return boolean_properties.scopes_route();
}

bool FlutterSemanticsNodeWrapper::HasNamesRoute() const {
  const BooleanProperties& boolean_properties = node_ptr_->boolean_properties();
  return boolean_properties.names_route();
}

bool FlutterSemanticsNodeWrapper::IsKeyboardNode() const {
  const BooleanProperties& boolean_properties = node_ptr_->boolean_properties();
  return boolean_properties.is_lift_to_type();
}

bool FlutterSemanticsNodeWrapper::CanBeAccessibilityFocused() const {
  // In Chrome, this means:
  // a node with a non-generic role and:
  // actionable nodes or top level scrollables with a name
  ui::AXNodeData data;
  PopulateAXRole(&data);
  bool non_generic_role = data.role != ax::mojom::Role::kGenericContainer &&
                          data.role != ax::mojom::Role::kGroup;
  bool actionable = node_ptr_->action_properties().tap() ||
                    node_ptr_->action_properties().long_press();
  bool top_level_scrollable =
      HasLabelHint() && (node_ptr_->action_properties().scroll_up() ||
                         node_ptr_->action_properties().scroll_down() ||
                         node_ptr_->action_properties().scroll_left() ||
                         node_ptr_->action_properties().scroll_right());
  return non_generic_role && (actionable || top_level_scrollable);
}

void FlutterSemanticsNodeWrapper::PopulateAXRole(
    ui::AXNodeData* out_data) const {
  const BooleanProperties& boolean_properties = node_ptr_->boolean_properties();
  const ActionProperties& action_properties = node_ptr_->action_properties();

  if (boolean_properties.is_text_field()) {
    out_data->role = ax::mojom::Role::kTextField;
    return;
  }

  if (boolean_properties.is_header()) {
    out_data->role = ax::mojom::Role::kHeading;
    return;
  }

  // b/148808637: Flutter allows buttons to be containers but ChromeVox
  // expects buttons to be atomic.  If we allow this node to be marked a
  // button and it contains other clickable nodes, none of those nodes will
  // be focusable by the user. This means no button that is also a
  // container of other actionable items will ever be read as 'button'
  // by the screen reader.  However, buttons that have no actionable
  // descendants will still say 'Button' as expected.
  if (boolean_properties.is_button() && !AnyChildIsActionable() &&
      HasLabelHint() && GetLabelHint().length() > 0) {
    out_data->role = ax::mojom::Role::kButton;
    return;
  }

  if (boolean_properties.is_image() &&
      node_ptr_->child_node_ids().size() == 0) {
    out_data->role = ax::mojom::Role::kImage;
    return;
  }

  if (action_properties.increase() || action_properties.decrease()) {
    out_data->role = ax::mojom::Role::kSlider;
    return;
  }

  bool has_checked_state = boolean_properties.has_checked_state();
  bool has_toggled_state = boolean_properties.has_toggled_state();

  if (has_checked_state) {
    if (boolean_properties.is_in_mutually_exclusive_group()) {
      out_data->role = ax::mojom::Role::kRadioButton;
    } else {
      out_data->role = ax::mojom::Role::kCheckBox;
    }
    return;
  }
  if (has_toggled_state) {
    out_data->role = ax::mojom::Role::kSwitch;
    return;
  }

  // b/149934151 : Flutter sends us nodes with labels that
  // have children. Don't mark these as static text or
  // no children will ever be focused properly via swipe
  // navigation. Only nodes that have labels with no children
  // should get the static text role. Use kHeader role
  // instead as it is allowed to be a container.
  if (HasLabelHint() && GetLabelHint().length() > 0) {
    if (node_ptr_->child_node_ids().size() == 0) {
      if (HasTapOrPress()) {
        out_data->role = ax::mojom::Role::kButton;
      } else {
        out_data->role = ax::mojom::Role::kStaticText;
      }
    } else {
      if (IsListItem()) {
        out_data->role = ax::mojom::Role::kListBoxOption;
      } else {
        out_data->role = ax::mojom::Role::kHeader;
      }
    }
    return;
  }

  std::vector<FlutterSemanticsNodeWrapper*> actionable_children;
  GetActionableChildren(&actionable_children);
  if (node_ptr_->scroll_children() > 0 && actionable_children.size() > 0) {
    out_data->role = ax::mojom::Role::kList;
    return;
  }

  out_data->role = ax::mojom::Role::kGenericContainer;
}

FlutterSemanticsNodeWrapper* FlutterSemanticsNodeWrapper::IsListItem() const {
  // To consider it a list item, the node has to have a ancestor that has scroll
  // children.
  std::vector<FlutterSemanticsNodeWrapper*> ancestors;
  FlutterSemanticsNodeWrapper* node = static_cast<FlutterSemanticsNodeWrapper*>(
      tree_source_->GetFromId(GetId()));

  while (node) {
    FlutterSemanticsNodeWrapper* parent =
        static_cast<FlutterSemanticsNodeWrapper*>(
            tree_source_->GetParent(node));
    if (parent)
      ancestors.push_back(parent);
    node = parent;
  }

  // |ancestors| is with order from closest ancestor to root. Find the closest
  // ancestor that has scroll children and in between there is no actionable
  // nodes.
  for (FlutterSemanticsNodeWrapper* ancestor : ancestors) {
    if (!ancestor->IsActionable()) {
      if (ancestor->node()->scroll_children() > 0) {
        return ancestor;
      }
    } else {
      break;
    }
  }

  return nullptr;
}

void FlutterSemanticsNodeWrapper::GetActionableChildren(
    std::vector<FlutterSemanticsNodeWrapper*>* out_children) const {
  std::vector<FlutterSemanticsNode*> children;
  GetChildren(&children);

  for (FlutterSemanticsNode* child : children) {
    FlutterSemanticsNodeWrapper* child_wrapper =
        static_cast<FlutterSemanticsNodeWrapper*>(child);

    if (child_wrapper->HasTapOrPress() ||
        child_wrapper->AnyChildIsActionable()) {
      out_children->push_back(child_wrapper);
    }
  }
}

bool FlutterSemanticsNodeWrapper::IsDescendant(
    FlutterSemanticsNodeWrapper* ancestor) const {
  FlutterSemanticsNodeWrapper* parent =
      static_cast<FlutterSemanticsNodeWrapper*>(
          tree_source_->GetParent(tree_source_->GetFromId(GetId())));

  while (parent) {
    if (parent == ancestor)
      return true;
    parent = static_cast<FlutterSemanticsNodeWrapper*>(
        tree_source_->GetParent(parent));
  }

  return false;
}

bool FlutterSemanticsNodeWrapper::IsRapidChangingSlider() const {
  const float min = node_ptr_->scroll_extent_min();
  const float max = node_ptr_->scroll_extent_max();
  bool has_scroll_extent = IsScrollable() && (min < max);
  return has_scroll_extent || (node_ptr_->action_properties().increase() ||
                               node_ptr_->action_properties().decrease());
}

void FlutterSemanticsNodeWrapper::PopulateAXState(
    ui::AXNodeData* out_data) const {
  const BooleanProperties& boolean_properties = node_ptr_->boolean_properties();

  if (boolean_properties.is_obscured()) {
    out_data->AddState(ax::mojom::State::kProtected);
  }

  if (boolean_properties.is_text_field()) {
    out_data->AddState(ax::mojom::State::kEditable);
  }

  if (IsFocusable()) {
    out_data->AddState(ax::mojom::State::kFocusable);
  }

  if (boolean_properties.has_checked_state()) {
    out_data->SetCheckedState(boolean_properties.is_checked()
                                  ? ax::mojom::CheckedState::kTrue
                                  : ax::mojom::CheckedState::kFalse);
  }

  if (boolean_properties.has_toggled_state()) {
    out_data->SetCheckedState(boolean_properties.is_toggled()
                                  ? ax::mojom::CheckedState::kTrue
                                  : ax::mojom::CheckedState::kFalse);
  }

  if (boolean_properties.has_enabled_state() &&
      !boolean_properties.is_enabled()) {
    out_data->SetRestriction(ax::mojom::Restriction::kDisabled);
  }
}

void FlutterSemanticsNodeWrapper::Serialize(ui::AXNodeData* out_data) const {
  PopulateAXState(out_data);

  const BooleanProperties& boolean_properties = node_ptr_->boolean_properties();
  const ActionProperties& action_properties = node_ptr_->action_properties();

  // b/162311902: For nodes that have scroll extents and can be changed rapidly,
  // set the name as empty so that ChromeVox will skip speaking them.
  if (IsRapidChangingSlider()) {
    out_data->SetName("");
  } else if (HasLabelHint()) {
    out_data->SetName(GetLabelHint());
  } else if (IsActionable()) {
    // Compute the name by joining all nodes with names.
    std::vector<std::string> names;
    ComputeNameFromContents(&names);
    if (!names.empty())
      out_data->SetName(base::JoinString(names, " "));
  }

  if (HasValue()) {
    out_data->SetValue(GetValue());
  }

  if (IsActionable()) {
    out_data->SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kClick);
  }
  out_data->AddBoolAttribute(ax::mojom::BoolAttribute::kScrollable,
                             IsScrollable());

  if (boolean_properties.is_selected()) {
    out_data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  }

  if (node_ptr_->has_hint()) {
    out_data->AddStringAttribute(ax::mojom::StringAttribute::kPlaceholder,
                                 node_ptr_->hint());
  }

  // Set bounds
  if (tree_source_->GetRoot()->GetId() != -1) {
    // TODO(rmrossi) Pass in nullptr for now for active window. Root node will
    // get bounds relative to 0,0 anyway since we are full screen.  This may
    // change if flutter is ever not full screen in which case we will have
    // to pass in the bounds of whatever container it resides in.
    const gfx::Rect& local_bounds = GetRelativeBounds();
    out_data->relative_bounds.bounds.SetRect(local_bounds.x(), local_bounds.y(),
                                             local_bounds.width(),
                                             local_bounds.height());
  }

  if (node_ptr_->has_text_selection_base()) {
    out_data->AddIntAttribute(ax::mojom::IntAttribute::kTextSelStart,
                              node_ptr_->text_selection_base());
  }

  if (node_ptr_->has_text_selection_extent()) {
    out_data->AddIntAttribute(ax::mojom::IntAttribute::kTextSelEnd,
                              node_ptr_->text_selection_extent());
  }

  if (action_properties.has_scroll_left() ||
      action_properties.has_scroll_up()) {
    out_data->AddAction(ax::mojom::Action::kScrollBackward);
  }

  if (action_properties.has_scroll_right() ||
      action_properties.has_scroll_down()) {
    out_data->AddAction(ax::mojom::Action::kScrollForward);
  }

  if (action_properties.set_selection()) {
    out_data->AddAction(ax::mojom::Action::kSetSelection);
  }

  if (action_properties.increase()) {
    out_data->AddAction(ax::mojom::Action::kIncrement);
  }

  if (action_properties.decrease()) {
    out_data->AddAction(ax::mojom::Action::kDecrement);
  }

  if (IsScrollable()) {
    const float position = node_ptr_->scroll_position();
    const float min = node_ptr_->scroll_extent_min();
    const float max = node_ptr_->scroll_extent_max();
    if (node_ptr_->action_properties().scroll_up() ||
        node_ptr_->action_properties().scroll_down()) {
      out_data->AddIntAttribute(ax::mojom::IntAttribute::kScrollY, position);
      out_data->AddIntAttribute(ax::mojom::IntAttribute::kScrollYMin, min);
      out_data->AddIntAttribute(ax::mojom::IntAttribute::kScrollYMax, max);
    } else if (node_ptr_->action_properties().scroll_left() ||
               node_ptr_->action_properties().scroll_right()) {
      out_data->AddIntAttribute(ax::mojom::IntAttribute::kScrollX, position);
      out_data->AddIntAttribute(ax::mojom::IntAttribute::kScrollXMin, min);
      out_data->AddIntAttribute(ax::mojom::IntAttribute::kScrollXMax, max);
    }
  }

  if (node_ptr_->custom_actions_size() > 0) {
    std::vector<int32_t> ids;
    std::vector<std::string> labels;
    for (int i = 0; i < node_ptr_->custom_actions_size(); i++) {
      ids.push_back(node_ptr_->custom_actions(i).id());
      labels.push_back(node_ptr_->custom_actions(i).label());
    }
    out_data->AddAction(ax::mojom::Action::kCustomAction);
    out_data->AddIntListAttribute(ax::mojom::IntListAttribute::kCustomActionIds,
                                  ids);
    out_data->AddStringListAttribute(
        ax::mojom::StringListAttribute::kCustomActionDescriptions, labels);
  }

  if (node_ptr_->has_plugin_id()) {
    std::string ax_tree_id = node_ptr_->plugin_id();
    if (ax_tree_id.rfind("T:", 0) == 0) {
      // This is a cast web contents id. Find the matching
      // CastWebContents and find the ax tree id from there.
      int web_contents_id;
      base::StringToInt(ax_tree_id.substr(2), &web_contents_id);
      std::vector<CastWebContents*> all_contents = CastWebContents::GetAll();
      // There will likely only ever be one active at any time.
      for (CastWebContents* contents : all_contents) {
        if (contents->id() == web_contents_id) {
          auto child_tree_id =
              contents->web_contents()->GetMainFrame()->GetAXTreeID();
          if (!child_tree_id.ToString().empty()) {
            out_data->AddChildTreeId(child_tree_id);
          }
          break;
        }
      }
    } else {
      // Use the value as a tree id.
      ui::AXTreeID child_ax_tree_id = ui::AXTreeID::FromString(ax_tree_id);
      out_data->AddChildTreeId(child_ax_tree_id);
    }
  }

  if (boolean_properties.is_live_region()) {
    out_data->AddStringAttribute(
        ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
    out_data->AddStringAttribute(
        ax::mojom::StringAttribute::kContainerLiveRelevant, "text");
  }

  if (out_data->role == ax::mojom::Role::kListBoxOption) {
    // Find the ancestor whose role is kList.
    FlutterSemanticsNodeWrapper* ancestor = IsListItem();

    std::vector<FlutterSemanticsNodeWrapper*> ancestor_actionable_children;
    ancestor->GetActionableChildren(&ancestor_actionable_children);

    // kSetSize should be the number of actionable children that the scrollable
    // ancestor has.
    out_data->AddIntAttribute(ax::mojom::IntAttribute::kSetSize,
                              ancestor_actionable_children.size());

    // Find which children tree that this node is in.
    for (size_t i = 0; i < ancestor_actionable_children.size(); ++i) {
      if (IsDescendant(ancestor_actionable_children[i])) {
        out_data->AddIntAttribute(ax::mojom::IntAttribute::kPosInSet, i + 1);
        break;
      }
    }
  }

  if (out_data->role == ax::mojom::Role::kList) {
    // Find the size of actionable children.
    std::vector<FlutterSemanticsNodeWrapper*> actionable_children;
    GetActionableChildren(&actionable_children);

    out_data->AddIntAttribute(ax::mojom::IntAttribute::kSetSize,
                              actionable_children.size());
  }
}

void FlutterSemanticsNodeWrapper::GetChildren(
    std::vector<FlutterSemanticsNode*>* children) const {
  for (auto id : node_ptr_->child_node_ids()) {
    FlutterSemanticsNode* node = tree_source_->GetFromId(id);
    if (node) {
      children->push_back(tree_source_->GetFromId(id));
    } else {
      LOG(ERROR) << "Node id present for which there is no child node given";
    }
  }
}

bool FlutterSemanticsNodeWrapper::AnyChildIsActionable() const {
  // b/156940104 - If this node is host to a child tree, we
  // can assume at least one of it's child nodes is actionable.
  // Otherwise, none of it's descendants will be traversed by
  // the reader.
  if (node_ptr_->has_plugin_id()) {
    return true;
  }
  for (auto child_id : node_ptr_->child_node_ids()) {
    FlutterSemanticsNodeWrapper* child =
        static_cast<FlutterSemanticsNodeWrapper*>(
            tree_source_->GetFromId(child_id));
    if (child->HasTapOrPress()) {
      return true;
    }
    if (child->AnyChildIsActionable()) {
      return true;
    }
  }
  return false;
}

bool FlutterSemanticsNodeWrapper::HasTapOrPress() const {
  return node_ptr_->boolean_properties().is_button() ||
         node_ptr_->action_properties().tap() ||
         node_ptr_->action_properties().long_press();
}

bool FlutterSemanticsNodeWrapper::IsActionable() const {
  bool actionable = HasTapOrPress();

  // If this node is actionable but is also the host for a child tree,
  // don't make it actionable or else chromevox won't traverse into
  // any child.
  if (node_ptr_->has_plugin_id()) {
    actionable = false;
  }
  return actionable;
}

bool FlutterSemanticsNodeWrapper::IsScrollable() const {
  return node_ptr_->action_properties().scroll_up() ||
         node_ptr_->action_properties().scroll_down() ||
         node_ptr_->action_properties().scroll_left() ||
         node_ptr_->action_properties().scroll_right();
}

bool FlutterSemanticsNodeWrapper::IsFocusable() const {
  const BooleanProperties& boolean_properties = node_ptr_->boolean_properties();
  if (boolean_properties.scopes_route() && !boolean_properties.names_route())
    return false;

  bool focusable_flags =
      boolean_properties.has_checked_state() ||
      boolean_properties.is_checked() || boolean_properties.is_selected() ||
      HasTapOrPress() || boolean_properties.is_text_field() ||
      boolean_properties.is_focused() ||
      boolean_properties.has_enabled_state() ||
      boolean_properties.is_enabled() ||
      boolean_properties.is_in_mutually_exclusive_group() ||
      boolean_properties.is_header() || boolean_properties.is_obscured() ||
      boolean_properties.names_route() ||
      (boolean_properties.is_image() &&
       node_ptr_->child_node_ids().size() == 0) ||
      boolean_properties.is_live_region() ||
      boolean_properties.has_toggled_state() || boolean_properties.is_toggled();

  // b/149934151 : If a node has a label but also has children, don't
  // mark it focusable, otherwise none of its children will be navigable
  // via swipe nav. It's role wlil be a generic container (see above).
  return focusable_flags || (HasLabelHint() && !GetLabelHint().empty() &&
                             node_ptr_->child_node_ids().size() == 0);
}

bool FlutterSemanticsNodeWrapper::HasLabelHint() const {
  return node_ptr_->has_label() || node_ptr_->has_hint();
}

std::string FlutterSemanticsNodeWrapper::GetLabelHint() const {
  // TODO(rmrossi): Find out whether this order of precedence makes sense
  if (node_ptr_->has_label() && node_ptr_->label().length() > 0) {
    return node_ptr_->label();
  }
  if (node_ptr_->has_hint() && node_ptr_->hint().length() > 0) {
    return node_ptr_->hint();
  }
  return "";
}

void FlutterSemanticsNodeWrapper::ComputeNameFromContents(
    std::vector<std::string>* names) const {
  DCHECK(names);
  std::string name;
  if (HasLabelHint()) {
    name = GetLabelHint();
    if (!name.empty()) {
      names->push_back(name);
      return;
    }
  }

  std::vector<FlutterSemanticsNode*> children;
  GetChildren(&children);
  for (const FlutterSemanticsNode* child : children) {
    child->ComputeNameFromContents(names);
  }
}

bool FlutterSemanticsNodeWrapper::HasValue() const {
  return node_ptr_->has_value();
}

std::string FlutterSemanticsNodeWrapper::GetValue() const {
  return node_ptr_->value();
}

const gfx::Rect FlutterSemanticsNodeWrapper::GetRelativeBounds() const {
  FlutterSemanticsNode* root_node = tree_source_->GetRoot();
  DCHECK(root_node);

  gfx::Rect node_bounds = GetBounds();

  // TODO(rmrossi): If embedded flutter is ever not full screen, we will have
  // to pass in the embedded object tag's screen coordinates to this function
  // and set the offset of the root node here separately from other nodes.
  // The bounds of the root node are supposed to be relative to its container
  // but since we are full screen, we leave them alone.  See
  // ax_tree_source_arc.cc for an example.
  if (GetId() != root_node->GetId()) {
    // Bounds of non-root node is relative to its tree's root.
    gfx::Rect root_bounds = root_node->GetBounds();
    node_bounds.Offset(-1 * root_bounds.x(), -1 * root_bounds.y());
  }

  return node_bounds;
}

}  // namespace accessibility
}  // namespace chromecast
