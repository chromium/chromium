// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/extensions_api/cast_extension_messages.h"
#include "chromecast/renderer/extensions/automation_internal_custom_bindings.h"
#include "ui/accessibility/ax_node.h"

namespace extensions {
namespace cast {

namespace {

// Convert from ax::mojom::Event to api::automation::EventType.
api::automation::EventType ToAutomationEvent(ax::mojom::Event event_type) {
  switch (event_type) {
    case ax::mojom::Event::kNone:
      return api::automation::EVENT_TYPE_NONE;
    case ax::mojom::Event::kActiveDescendantChanged:
      return api::automation::EVENT_TYPE_ACTIVEDESCENDANTCHANGED;
    case ax::mojom::Event::kAlert:
      return api::automation::EVENT_TYPE_ALERT;
    case ax::mojom::Event::kAriaAttributeChanged:
      return api::automation::EVENT_TYPE_ARIAATTRIBUTECHANGED;
    case ax::mojom::Event::kAutocorrectionOccured:
      return api::automation::EVENT_TYPE_AUTOCORRECTIONOCCURED;
    case ax::mojom::Event::kBlur:
      return api::automation::EVENT_TYPE_BLUR;
    case ax::mojom::Event::kCheckedStateChanged:
      return api::automation::EVENT_TYPE_CHECKEDSTATECHANGED;
    case ax::mojom::Event::kChildrenChanged:
      return api::automation::EVENT_TYPE_CHILDRENCHANGED;
    case ax::mojom::Event::kClicked:
      return api::automation::EVENT_TYPE_CLICKED;
    case ax::mojom::Event::kDocumentSelectionChanged:
      return api::automation::EVENT_TYPE_DOCUMENTSELECTIONCHANGED;
    case ax::mojom::Event::kDocumentTitleChanged:
      return api::automation::EVENT_TYPE_DOCUMENTTITLECHANGED;
    case ax::mojom::Event::kExpandedChanged:
      return api::automation::EVENT_TYPE_EXPANDEDCHANGED;
    case ax::mojom::Event::kFocus:
    case ax::mojom::Event::kFocusContext:
      return api::automation::EVENT_TYPE_FOCUS;
    case ax::mojom::Event::kHide:
      return api::automation::EVENT_TYPE_HIDE;
    case ax::mojom::Event::kHitTestResult:
      return api::automation::EVENT_TYPE_HITTESTRESULT;
    case ax::mojom::Event::kHover:
      return api::automation::EVENT_TYPE_HOVER;
    case ax::mojom::Event::kImageFrameUpdated:
      return api::automation::EVENT_TYPE_IMAGEFRAMEUPDATED;
    case ax::mojom::Event::kInvalidStatusChanged:
      return api::automation::EVENT_TYPE_INVALIDSTATUSCHANGED;
    case ax::mojom::Event::kLayoutComplete:
      return api::automation::EVENT_TYPE_LAYOUTCOMPLETE;
    case ax::mojom::Event::kLiveRegionCreated:
      return api::automation::EVENT_TYPE_LIVEREGIONCREATED;
    case ax::mojom::Event::kLiveRegionChanged:
      return api::automation::EVENT_TYPE_LIVEREGIONCHANGED;
    case ax::mojom::Event::kLoadComplete:
      return api::automation::EVENT_TYPE_LOADCOMPLETE;
    case ax::mojom::Event::kLoadStart:
      return api::automation::EVENT_TYPE_LOADSTART;
    case ax::mojom::Event::kLocationChanged:
      return api::automation::EVENT_TYPE_LOCATIONCHANGED;
    case ax::mojom::Event::kMediaStartedPlaying:
      return api::automation::EVENT_TYPE_MEDIASTARTEDPLAYING;
    case ax::mojom::Event::kMediaStoppedPlaying:
      return api::automation::EVENT_TYPE_MEDIASTOPPEDPLAYING;
    case ax::mojom::Event::kMenuEnd:
      return api::automation::EVENT_TYPE_MENUEND;
    case ax::mojom::Event::kMenuListItemSelected:
      return api::automation::EVENT_TYPE_MENULISTITEMSELECTED;
    case ax::mojom::Event::kMenuListValueChanged:
      return api::automation::EVENT_TYPE_MENULISTVALUECHANGED;
    case ax::mojom::Event::kMenuPopupEnd:
      return api::automation::EVENT_TYPE_MENUPOPUPEND;
    case ax::mojom::Event::kMenuPopupStart:
      return api::automation::EVENT_TYPE_MENUPOPUPSTART;
    case ax::mojom::Event::kMenuStart:
      return api::automation::EVENT_TYPE_MENUSTART;
    case ax::mojom::Event::kMouseCanceled:
      return api::automation::EVENT_TYPE_MOUSECANCELED;
    case ax::mojom::Event::kMouseDragged:
      return api::automation::EVENT_TYPE_MOUSEDRAGGED;
    case ax::mojom::Event::kMouseMoved:
      return api::automation::EVENT_TYPE_MOUSEMOVED;
    case ax::mojom::Event::kMousePressed:
      return api::automation::EVENT_TYPE_MOUSEPRESSED;
    case ax::mojom::Event::kMouseReleased:
      return api::automation::EVENT_TYPE_MOUSERELEASED;
    case ax::mojom::Event::kRowCollapsed:
      return api::automation::EVENT_TYPE_ROWCOLLAPSED;
    case ax::mojom::Event::kRowCountChanged:
      return api::automation::EVENT_TYPE_ROWCOUNTCHANGED;
    case ax::mojom::Event::kRowExpanded:
      return api::automation::EVENT_TYPE_ROWEXPANDED;
    case ax::mojom::Event::kScrollPositionChanged:
      return api::automation::EVENT_TYPE_SCROLLPOSITIONCHANGED;
    case ax::mojom::Event::kScrolledToAnchor:
      return api::automation::EVENT_TYPE_SCROLLEDTOANCHOR;
    case ax::mojom::Event::kSelectedChildrenChanged:
      return api::automation::EVENT_TYPE_SELECTEDCHILDRENCHANGED;
    case ax::mojom::Event::kSelection:
      return api::automation::EVENT_TYPE_SELECTION;
    case ax::mojom::Event::kSelectionAdd:
      return api::automation::EVENT_TYPE_SELECTIONADD;
    case ax::mojom::Event::kSelectionRemove:
      return api::automation::EVENT_TYPE_SELECTIONREMOVE;
    case ax::mojom::Event::kShow:
      return api::automation::EVENT_TYPE_SHOW;
    case ax::mojom::Event::kStateChanged:
      return api::automation::EVENT_TYPE_NONE;
    case ax::mojom::Event::kTextChanged:
      return api::automation::EVENT_TYPE_TEXTCHANGED;
    case ax::mojom::Event::kTextSelectionChanged:
      return api::automation::EVENT_TYPE_TEXTSELECTIONCHANGED;
    case ax::mojom::Event::kTreeChanged:
      return api::automation::EVENT_TYPE_TREECHANGED;
    case ax::mojom::Event::kValueChanged:
      return api::automation::EVENT_TYPE_VALUECHANGED;
  }

  NOTREACHED();
  return api::automation::EVENT_TYPE_NONE;
}

// Convert from ui::AXEventGenerator::Event to api::automation::EventType.
api::automation::EventType ToAutomationEvent(
    ui::AXEventGenerator::Event event_type) {
  switch (event_type) {
    case ui::AXEventGenerator::Event::ACTIVE_DESCENDANT_CHANGED:
      return api::automation::EVENT_TYPE_ACTIVEDESCENDANTCHANGED;
    case ui::AXEventGenerator::Event::ALERT:
      return api::automation::EVENT_TYPE_ALERT;
    case ui::AXEventGenerator::Event::CHECKED_STATE_CHANGED:
      return api::automation::EVENT_TYPE_CHECKEDSTATECHANGED;
    case ui::AXEventGenerator::Event::CHILDREN_CHANGED:
      return api::automation::EVENT_TYPE_CHILDRENCHANGED;
    case ui::AXEventGenerator::Event::DOCUMENT_SELECTION_CHANGED:
      return api::automation::EVENT_TYPE_DOCUMENTSELECTIONCHANGED;
    case ui::AXEventGenerator::Event::DOCUMENT_TITLE_CHANGED:
      return api::automation::EVENT_TYPE_DOCUMENTTITLECHANGED;
    case ui::AXEventGenerator::Event::INVALID_STATUS_CHANGED:
      return api::automation::EVENT_TYPE_INVALIDSTATUSCHANGED;
    case ui::AXEventGenerator::Event::LIVE_REGION_CHANGED:
      return api::automation::EVENT_TYPE_LIVEREGIONCHANGED;
    case ui::AXEventGenerator::Event::LIVE_REGION_CREATED:
      return api::automation::EVENT_TYPE_LIVEREGIONCREATED;
    case ui::AXEventGenerator::Event::LOAD_COMPLETE:
      return api::automation::EVENT_TYPE_LOADCOMPLETE;
    case ui::AXEventGenerator::Event::LOAD_START:
      return api::automation::EVENT_TYPE_LOADSTART;
    case ui::AXEventGenerator::Event::MENU_ITEM_SELECTED:
      return api::automation::EVENT_TYPE_MENULISTITEMSELECTED;
    case ui::AXEventGenerator::Event::RELATED_NODE_CHANGED:
      return api::automation::EVENT_TYPE_ARIAATTRIBUTECHANGED;
    case ui::AXEventGenerator::Event::ROW_COUNT_CHANGED:
      return api::automation::EVENT_TYPE_ROWCOUNTCHANGED;
    case ui::AXEventGenerator::Event::SCROLL_POSITION_CHANGED:
      return api::automation::EVENT_TYPE_SCROLLPOSITIONCHANGED;
    case ui::AXEventGenerator::Event::SELECTED_CHILDREN_CHANGED:
      return api::automation::EVENT_TYPE_SELECTEDCHILDRENCHANGED;
    case ui::AXEventGenerator::Event::VALUE_CHANGED:
      return api::automation::EVENT_TYPE_VALUECHANGED;

    // Map these into generic attribute changes (not necessarily aria related,
    // but mapping for backward compat).
    case ui::AXEventGenerator::Event::COLLAPSED:
    case ui::AXEventGenerator::Event::EXPANDED:
    case ui::AXEventGenerator::Event::LIVE_REGION_NODE_CHANGED:
    case ui::AXEventGenerator::Event::NAME_CHANGED:
    case ui::AXEventGenerator::Event::ROLE_CHANGED:
    case ui::AXEventGenerator::Event::SELECTED_CHANGED:
    case ui::AXEventGenerator::Event::STATE_CHANGED:
      return api::automation::EVENT_TYPE_ARIAATTRIBUTECHANGED;

    case ui::AXEventGenerator::Event::DESCRIPTION_CHANGED:
    case ui::AXEventGenerator::Event::OTHER_ATTRIBUTE_CHANGED:
      return api::automation::EVENT_TYPE_NONE;
  }

  NOTREACHED();
  return api::automation::EVENT_TYPE_NONE;
}

}  // namespace

AutomationAXTreeWrapper::AutomationAXTreeWrapper(
    ui::AXTreeID tree_id,
    AutomationInternalCustomBindings* owner)
    : tree_id_(tree_id), host_node_id_(-1), owner_(owner) {
  // We have to initialize AXEventGenerator here - we can't do it in the
  // initializer list because AXTree hasn't been initialized yet at that point.
  SetTree(&tree_);
}

AutomationAXTreeWrapper::~AutomationAXTreeWrapper() {
  // Clearing the delegate so we don't get a callback for every node
  // being deleted.
  tree_.SetDelegate(nullptr);
}

bool AutomationAXTreeWrapper::OnAccessibilityEvents(
    const ExtensionMsg_AccessibilityEventBundleParams& event_bundle,
    bool is_active_profile) {
  for (const auto& update : event_bundle.updates) {
    set_event_from(update.event_from);
    deleted_node_ids_.clear();

    if (!tree_.Unserialize(update))
      return false;

    if (is_active_profile) {
      owner_->SendNodesRemovedEvent(&tree_, deleted_node_ids_);

      if (update.nodes.size()) {
        ui::AXNode* target = tree_.GetFromId(update.nodes[0].id);
        if (target) {
          owner_->SendTreeChangeEvent(
              api::automation::TREE_CHANGE_TYPE_SUBTREEUPDATEEND, &tree_,
              target);
        }
      }
    }
  }

  // Exit early if this isn't the active profile.
  if (!is_active_profile)
    return true;

  // Send auto-generated AXEventGenerator events.
  for (auto targeted_event : *this) {
    api::automation::EventType event_type =
        ToAutomationEvent(targeted_event.event_params.event);
    if (IsEventTypeHandledByAXEventGenerator(event_type)) {
      ui::AXEvent generated_event;
      generated_event.id = targeted_event.node->id();
      generated_event.event_from = targeted_event.event_params.event_from;
      owner_->SendAutomationEvent(event_bundle.tree_id,
                                  event_bundle.mouse_location, generated_event,
                                  event_type);
    }
  }
  ClearEvents();

  for (auto event : event_bundle.events) {
    api::automation::EventType automation_event_type =
        ToAutomationEvent(event.event_type);

    // Send some events directly from the event message, if they're not
    // handled by AXEventGenerator yet.
    if (!IsEventTypeHandledByAXEventGenerator(automation_event_type)) {
      owner_->SendAutomationEvent(event_bundle.tree_id,
                                  event_bundle.mouse_location, event,
                                  automation_event_type);
    }
  }

  return true;
}

void AutomationAXTreeWrapper::OnNodeDataWillChange(
    ui::AXTree* tree,
    const ui::AXNodeData& old_node_data,
    const ui::AXNodeData& new_node_data) {
  AXEventGenerator::OnNodeDataWillChange(tree, old_node_data, new_node_data);
  if (old_node_data.GetStringAttribute(ax::mojom::StringAttribute::kName) !=
      new_node_data.GetStringAttribute(ax::mojom::StringAttribute::kName))
    text_changed_node_ids_.push_back(new_node_data.id);
}

void AutomationAXTreeWrapper::OnNodeWillBeDeleted(ui::AXTree* tree,
                                                  ui::AXNode* node) {
  AXEventGenerator::OnNodeWillBeDeleted(tree, node);
  owner_->SendTreeChangeEvent(api::automation::TREE_CHANGE_TYPE_NODEREMOVED,
                              tree, node);
  deleted_node_ids_.push_back(node->id());
}

void AutomationAXTreeWrapper::OnAtomicUpdateFinished(
    ui::AXTree* tree,
    bool root_changed,
    const std::vector<ui::AXTreeDelegate::Change>& changes) {
  AXEventGenerator::OnAtomicUpdateFinished(tree, root_changed, changes);
  DCHECK_EQ(&tree_, tree);
  for (const auto change : changes) {
    ui::AXNode* node = change.node;
    switch (change.type) {
      case NODE_CREATED:
        owner_->SendTreeChangeEvent(
            api::automation::TREE_CHANGE_TYPE_NODECREATED, tree, node);
        break;
      case SUBTREE_CREATED:
        owner_->SendTreeChangeEvent(
            api::automation::TREE_CHANGE_TYPE_SUBTREECREATED, tree, node);
        break;
      case NODE_CHANGED:
        owner_->SendTreeChangeEvent(
            api::automation::TREE_CHANGE_TYPE_NODECHANGED, tree, node);
        break;
      // Unhandled.
      case NODE_REPARENTED:
      case SUBTREE_REPARENTED:
        break;
    }
  }

  for (int id : text_changed_node_ids_) {
    owner_->SendTreeChangeEvent(api::automation::TREE_CHANGE_TYPE_TEXTCHANGED,
                                tree, tree->GetFromId(id));
  }
  text_changed_node_ids_.clear();
}

bool AutomationAXTreeWrapper::IsEventTypeHandledByAXEventGenerator(
    api::automation::EventType event_type) const {
  switch (event_type) {
    // Generated by AXEventGenerator.
    case api::automation::EVENT_TYPE_ACTIVEDESCENDANTCHANGED:
    case api::automation::EVENT_TYPE_ARIAATTRIBUTECHANGED:
    case api::automation::EVENT_TYPE_CHECKEDSTATECHANGED:
    case api::automation::EVENT_TYPE_DOCUMENTSELECTIONCHANGED:
    case api::automation::EVENT_TYPE_DOCUMENTTITLECHANGED:
    case api::automation::EVENT_TYPE_EXPANDEDCHANGED:
    case api::automation::EVENT_TYPE_INVALIDSTATUSCHANGED:
    case api::automation::EVENT_TYPE_LIVEREGIONCHANGED:
    case api::automation::EVENT_TYPE_LIVEREGIONCREATED:
    case api::automation::EVENT_TYPE_LOADCOMPLETE:
    case api::automation::EVENT_TYPE_LOADSTART:
    case api::automation::EVENT_TYPE_SCROLLPOSITIONCHANGED:
    case api::automation::EVENT_TYPE_SELECTEDCHILDRENCHANGED:
      return true;

    // Not generated by AXEventGenerator and possible candidates
    // for removal from the automation API entirely.
    case api::automation::EVENT_TYPE_HIDE:
    case api::automation::EVENT_TYPE_LAYOUTCOMPLETE:
    case api::automation::EVENT_TYPE_MENULISTVALUECHANGED:
    case api::automation::EVENT_TYPE_MENUPOPUPEND:
    case api::automation::EVENT_TYPE_MENUPOPUPSTART:
    case api::automation::EVENT_TYPE_SELECTIONADD:
    case api::automation::EVENT_TYPE_SELECTIONREMOVE:
    case api::automation::EVENT_TYPE_SHOW:
    case api::automation::EVENT_TYPE_STATECHANGED:
    case api::automation::EVENT_TYPE_TREECHANGED:
      return false;

    // These events will never be generated by AXEventGenerator.
    // These are all events that can't be inferred from a tree change.
    case api::automation::EVENT_TYPE_NONE:
    case api::automation::EVENT_TYPE_AUTOCORRECTIONOCCURED:
    case api::automation::EVENT_TYPE_CLICKED:
    case api::automation::EVENT_TYPE_FOCUSCONTEXT:
    case api::automation::EVENT_TYPE_HITTESTRESULT:
    case api::automation::EVENT_TYPE_HOVER:
    case api::automation::EVENT_TYPE_MEDIASTARTEDPLAYING:
    case api::automation::EVENT_TYPE_MEDIASTOPPEDPLAYING:
    case api::automation::EVENT_TYPE_MOUSECANCELED:
    case api::automation::EVENT_TYPE_MOUSEDRAGGED:
    case api::automation::EVENT_TYPE_MOUSEMOVED:
    case api::automation::EVENT_TYPE_MOUSEPRESSED:
    case api::automation::EVENT_TYPE_MOUSERELEASED:
    case api::automation::EVENT_TYPE_SCROLLEDTOANCHOR:
      return false;

    // These events might need to be migrated to AXEventGenerator.
    case api::automation::EVENT_TYPE_ALERT:
    case api::automation::EVENT_TYPE_BLUR:
    case api::automation::EVENT_TYPE_CHILDRENCHANGED:
    case api::automation::EVENT_TYPE_FOCUS:
    case api::automation::EVENT_TYPE_IMAGEFRAMEUPDATED:
    case api::automation::EVENT_TYPE_LOCATIONCHANGED:
    case api::automation::EVENT_TYPE_MENUEND:
    case api::automation::EVENT_TYPE_MENULISTITEMSELECTED:
    case api::automation::EVENT_TYPE_MENUSTART:
    case api::automation::EVENT_TYPE_ROWCOLLAPSED:
    case api::automation::EVENT_TYPE_ROWCOUNTCHANGED:
    case api::automation::EVENT_TYPE_ROWEXPANDED:
    case api::automation::EVENT_TYPE_SELECTION:
    case api::automation::EVENT_TYPE_TEXTCHANGED:
    case api::automation::EVENT_TYPE_TEXTSELECTIONCHANGED:
    case api::automation::EVENT_TYPE_VALUECHANGED:
      return false;
  }

  NOTREACHED();
  return false;
}

}  // namespace cast
}  // namespace extensions
