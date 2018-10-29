// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var AutomationEvent = require('automationEvent').AutomationEvent;
var automationInternal =
    getInternalApi ?
        getInternalApi('automationInternal') :
        require('binding').Binding.create('automationInternal').generate();
var exceptionHandler = require('uncaught_exception_handler');

var natives = requireNative('automationInternal');

var IsInteractPermitted = natives.IsInteractPermitted;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @return {?number} The id of the root node.
 */
var GetRootID = natives.GetRootID;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @return {?string} The title of the document.
 */
var GetDocTitle = natives.GetDocTitle;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @return {?string} The url of the document.
 */
var GetDocURL = natives.GetDocURL;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @return {?boolean} True if the document has finished loading.
 */
var GetDocLoaded = natives.GetDocLoaded;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @return {?number} The loading progress, from 0.0 to 1.0 (fully loaded).
 */
var GetDocLoadingProgress =
    natives.GetDocLoadingProgress;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @return {?number} The ID of the selection anchor object.
 */
var GetAnchorObjectID = natives.GetAnchorObjectID;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @return {?number} The selection anchor offset.
 */
var GetAnchorOffset = natives.GetAnchorOffset;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @return {?string} The selection anchor affinity.
 */
var GetAnchorAffinity = natives.GetAnchorAffinity;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @return {?number} The ID of the selection focus object.
 */
var GetFocusObjectID = natives.GetFocusObjectID;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @return {?number} The selection focus offset.
 */
var GetFocusOffset = natives.GetFocusOffset;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @return {?string} The selection focus affinity.
 */
var GetFocusAffinity = natives.GetFocusAffinity;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {?number} The id of the node's parent, or undefined if it's the
 *    root of its tree or if the tree or node wasn't found.
 */
var GetParentID = natives.GetParentID;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {?number} The number of children of the node, or undefined if
 *     the tree or node wasn't found.
 */
var GetChildCount = natives.GetChildCount;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @param {number} childIndex An index of a child of this node.
 * @return {?number} The id of the child at the given index, or undefined
 *     if the tree or node or child at that index wasn't found.
 */
var GetChildIDAtIndex = natives.GetChildIDAtIndex;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {?number} The ids of the children of the node, or undefined
 *     if the tree or node wasn't found.
 */
var GetChildIds = natives.GetChildIDs;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {?Object} An object mapping html attributes to values.
 */
var GetHtmlAttributes = natives.GetHtmlAttributes;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {?number} The index of this node in its parent, or undefined if
 *     the tree or node or node parent wasn't found.
 */
var GetIndexInParent = natives.GetIndexInParent;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {?Object} An object with a string key for every state flag set,
 *     or undefined if the tree or node or node parent wasn't found.
 */
var GetState = natives.GetState;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {string} The restriction, one of
 * "disabled", "readOnly" or undefined if enabled or other object not disabled
 */
var GetRestriction = natives.GetRestriction;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {string} The checked state, as undefined, "true", "false" or "mixed".
 */
var GetChecked = natives.GetChecked;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {string} The role of the node, or undefined if the tree or
 *     node wasn't found.
 */
var GetRole = natives.GetRole;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {?automation.Rect} The location of the node, or undefined if
 *     the tree or node wasn't found.
 */
var GetLocation = natives.GetLocation;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @param {number} startIndex The start index of the range.
 * @param {number} endIndex The end index of the range.
 * @return {?automation.Rect} The bounding box of the subrange of this node,
 *     or the location if there are no subranges, or undefined if
 *     the tree or node wasn't found.
 */
var GetBoundsForRange = natives.GetBoundsForRange;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {?automation.Rect} The unclipped location of the node, or
 * undefined if the tree or node wasn't found.
 */
var GetUnclippedLocation = natives.GetUnclippedLocation;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {!Array<number>} The text offset where each line starts, or an empty
 *     array if this node has no text content, or undefined if the tree or node
 *     was not found.
 */
var GetLineStartOffsets = requireNative(
    'automationInternal').GetLineStartOffsets;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @param {string} attr The name of a string attribute.
 * @return {?string} The value of this attribute, or undefined if the tree,
 *     node, or attribute wasn't found.
 */
var GetStringAttribute = natives.GetStringAttribute;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @param {string} attr The name of an attribute.
 * @return {?boolean} The value of this attribute, or undefined if the tree,
 *     node, or attribute wasn't found.
 */
var GetBoolAttribute = natives.GetBoolAttribute;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @param {string} attr The name of an attribute.
 * @return {?number} The value of this attribute, or undefined if the tree,
 *     node, or attribute wasn't found.
 */
var GetIntAttribute = natives.GetIntAttribute;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @param {string} attr The name of an attribute.
 * @return {?Array<number>} The ids of nodes who have a relationship pointing
 *     to |nodeID| (a reverse relationship).
 */
var GetIntAttributeReverseRelations =
    natives.GetIntAttributeReverseRelations;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @param {string} attr The name of an attribute.
 * @return {?number} The value of this attribute, or undefined if the tree,
 *     node, or attribute wasn't found.
 */
var GetFloatAttribute = natives.GetFloatAttribute;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @param {string} attr The name of an attribute.
 * @return {?Array<number>} The value of this attribute, or undefined
 *     if the tree, node, or attribute wasn't found.
 */
var GetIntListAttribute =
    natives.GetIntListAttribute;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @param {string} attr The name of an attribute.
 * @return {?Array<number>} The ids of nodes who have a relationship pointing
 *     to |nodeID| (a reverse relationship).
 */
var GetIntListAttributeReverseRelations =
    natives.GetIntListAttributeReverseRelations;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @param {string} attr The name of an HTML attribute.
 * @return {?string} The value of this attribute, or undefined if the tree,
 *     node, or attribute wasn't found.
 */
var GetHtmlAttribute = natives.GetHtmlAttribute;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {automation.NameFromType} The source of the node's name.
 */
var GetNameFrom = natives.GetNameFrom;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {boolean}
 */
var GetBold = natives.GetBold;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {boolean}
 */
var GetItalic = natives.GetItalic;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {boolean}
 */
var GetUnderline = natives.GetUnderline;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {boolean}
 */
var GetLineThrough = natives.GetLineThrough;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {?Array<automation.CustomAction>} List of custom actions of the
 *     node.
 */
var GetCustomActions = natives.GetCustomActions;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {?Array<string>} List of standard actions of the node.
 */
var GetStandardActions = natives.GetStandardActions;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {automation.NameFromType} The source of the node's name.
 */
var GetDefaultActionVerb = natives.GetDefaultActionVerb;


/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @param {string} searchStr
 * @param {boolean} backward
 * @return {{treeId: string, nodeId: number}}
 */
var GetNextTextMatch = natives.GetNextTextMatch;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {?Array<number>} A list of column header ids.

 * @return {?number} The id of the column header, if it exists.
 */
var GetTableCellColumnHeaders = natives.GetTableCellColumnHeaders;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {?Array<number>} A list of row header ids.
 */
var GetTableCellRowHeaders = natives.GetTableCellRowHeaders;

var logging = requireNative('logging');
var utils = require('utils');

/**
 * A single node in the Automation tree.
 * @param {AutomationRootNodeImpl} root The root of the tree.
 * @constructor
 */
function AutomationNodeImpl(root) {
  this.rootImpl = root;
  this.listeners = {__proto__: null};
}

AutomationNodeImpl.prototype = {
  __proto__: null,
  treeID: "",
  id: -1,
  isRootNode: false,

  detach: function() {
    this.rootImpl = null;
    this.listeners = {__proto__: null};
  },

  get root() {
    return this.rootImpl && this.rootImpl.wrapper;
  },

  get parent() {
    var info = GetParentID(this.treeID, this.id);
    if (!info)
      return;
    return AutomationRootNodeImpl.getNodeFromTree(info.treeId, info.nodeId);
  },

  get htmlAttributes() {
    return GetHtmlAttributes(this.treeID, this.id) || {};
  },

  get state() {
    return GetState(this.treeID, this.id) || {};
  },

  get role() {
    return GetRole(this.treeID, this.id);
  },

  get restriction() {
    return GetRestriction(this.treeID, this.id);
  },

  get checked() {
    return GetChecked(this.treeID, this.id);
  },

  get location() {
    return GetLocation(this.treeID, this.id);
  },

  boundsForRange: function(startIndex, endIndex) {
    return GetBoundsForRange(this.treeID, this.id, startIndex, endIndex);
  },

  get unclippedLocation() {
    var result = GetUnclippedLocation(this.treeID, this.id);
    if (result === undefined)
      result = GetLocation(this.treeID, this.id);
    return result;
  },

  get indexInParent() {
    return GetIndexInParent(this.treeID, this.id);
  },

  get lineStartOffsets() {
    return GetLineStartOffsets(this.treeID, this.id);
  },

  get childTree() {
    var childTreeID = GetStringAttribute(this.treeID, this.id, 'childTreeId');
    if (childTreeID)
      return AutomationRootNodeImpl.get(childTreeID);
  },

  get firstChild() {
    if (GetChildCount(this.treeID, this.id) == 0)
      return undefined;
    var info = GetChildIDAtIndex(this.treeID, this.id, 0);
    if (info)
      return AutomationRootNodeImpl.getNodeFromTree(info.treeId, info.nodeId);
  },

  get lastChild() {
    var count = GetChildCount(this.treeID, this.id);
    if (count == 0)
      return;

    var info = GetChildIDAtIndex(this.treeID, this.id, count - 1);
    if (info)
      return AutomationRootNodeImpl.getNodeFromTree(info.treeId, info.nodeId);
  },

  get children() {
    var info = GetChildIds(this.treeID, this.id);
    if (!info)
      return [];

    var children = [];
    for (var i = 0; i < info.nodeIds.length; ++i) {
      var childID = info.nodeIds[i];
      var child = AutomationRootNodeImpl.getNodeFromTree(info.treeId, childID);
      if (child)
        $Array.push(children, child);
    }
    return children;
  },

  get previousSibling() {
    var parent = this.parent;
    if (!parent)
      return undefined;
    parent = privates(parent).impl;
    var indexInParent = GetIndexInParent(this.treeID, this.id);
    var info = GetChildIDAtIndex(parent.treeID, parent.id, indexInParent - 1);
    if (info)
      return AutomationRootNodeImpl.getNodeFromTree(info.treeId, info.nodeId);
  },

  get nextSibling() {
    var parent = this.parent;
    if (!parent)
      return undefined;
    parent = privates(parent).impl;
    var indexInParent = GetIndexInParent(this.treeID, this.id);
    var info = GetChildIDAtIndex(parent.treeID, parent.id, indexInParent + 1);
    if (info)
      return AutomationRootNodeImpl.getNodeFromTree(info.treeId, info.nodeId);
  },

  get nameFrom() {
    return GetNameFrom(this.treeID, this.id);
  },

  get bold() {
    return GetBold(this.treeID, this.id);
  },

  get italic() {
    return GetItalic(this.treeID, this.id);
  },

  get underline() {
    return GetUnderline(this.treeID, this.id);
  },

  get lineThrough() {
    return GetLineThrough(this.treeID, this.id);
  },

  get customActions() {
    return GetCustomActions(this.treeID, this.id);
  },

  get standardActions() {
    return GetStandardActions(this.treeID, this.id);
  },

  get defaultActionVerb() {
    return GetDefaultActionVerb(this.treeID, this.id);
  },

  get tableCellColumnHeaders() {
    var ids = GetTableCellColumnHeaders(this.treeID, this.id);
    if (ids && this.rootImpl) {
      var result = [];
      for (var i = 0; i < ids.length; i++)
        result.push(this.rootImpl.get(ids[i]));
      return result;
    }
  },

  get tableCellRowHeaders() {
    var id = GetTableCellRowHeaders(this.treeID, this.id);
    if (ids && this.rootImpl) {
      var result = [];
      for (var i = 0; i < ids.length; i++)
        result.push(this.rootImpl.get(ids[i]));
      return result;
    }
  },

  doDefault: function() {
    this.performAction_('doDefault');
  },

  focus: function() {
    this.performAction_('focus');
  },

  getImageData: function(maxWidth, maxHeight) {
    this.performAction_('getImageData',
                        { maxWidth: maxWidth,
                          maxHeight: maxHeight });
  },

  hitTest: function(x, y, eventToFire) {
    this.hitTestInternal(x, y, eventToFire);
  },

  hitTestWithReply: function(x, y, opt_callback) {
    this.hitTestInternal(x, y, 'hitTestResult', opt_callback);
  },

  hitTestInternal: function(x, y, eventToFire, opt_callback) {
    // Convert from global to tree-relative coordinates.
    var location = GetLocation(this.treeID, GetRootID(this.treeID));
    this.performAction_('hitTest',
                        { x: Math.floor(x - location.left),
                          y: Math.floor(y - location.top),
                          eventToFire: eventToFire },
                        opt_callback);
  },

  makeVisible: function() {
    this.performAction_('scrollToMakeVisible');
  },

  performCustomAction: function(customActionId) {
    this.performAction_('customAction', { customActionID: customActionId });
  },

  performStandardAction: function(action) {
    var standardActions = GetStandardActions(this.treeID, this.id);
    if (!standardActions ||
        !standardActions.find(item => action == item)) {
      throw 'Inapplicable action for node: ' + action;
    }
    this.performAction_(action);
  },

  replaceSelectedText: function(value) {
    if (this.state.editable) {
      this.performAction_('replaceSelectedText', { value: value});
    }
  },

  resumeMedia: function() {
    this.performAction_('resumeMedia');
  },

  scrollBackward: function(opt_callback) {
    this.performAction_('scrollBackward', {}, opt_callback);
  },

  scrollForward: function(opt_callback) {
    this.performAction_('scrollForward', {}, opt_callback);
  },

  scrollUp: function(opt_callback) {
    this.performAction_('scrollUp', {}, opt_callback);
  },

  scrollDown: function(opt_callback) {
    this.performAction_('scrollDown', {}, opt_callback);
  },

  scrollLeft: function(opt_callback) {
    this.performAction_('scrollLeft', {}, opt_callback);
  },

  scrollRight: function(opt_callback) {
    this.performAction_('scrollRight', {}, opt_callback);
  },

  setSelection: function(startIndex, endIndex) {
    if (this.state.editable) {
      this.performAction_('setSelection',
                          { focusNodeID: this.id,
                            anchorOffset: startIndex,
                            focusOffset: endIndex });
    }
  },

  setSequentialFocusNavigationStartingPoint: function() {
    this.performAction_('setSequentialFocusNavigationStartingPoint');
  },

  setValue: function(value) {
    if (this.state.editable) {
      this.performAction_('setValue', { value: value});
    }
  },

  showContextMenu: function() {
    this.performAction_('showContextMenu');
  },

  startDuckingMedia: function() {
    this.performAction_('startDuckingMedia');
  },

  stopDuckingMedia: function() {
    this.performAction_('stopDuckingMedia');
  },

  suspendMedia: function() {
    this.performAction_('suspendMedia');
  },

  domQuerySelector: function(selector, callback) {
    if (!this.rootImpl)
      callback();
    automationInternal.querySelector(
      { treeID: this.rootImpl.treeID,
        automationNodeID: this.id,
        selector: selector },
      $Function.bind(this.domQuerySelectorCallback_, this, callback));
  },

  find: function(params) {
    return this.findInternal_(params);
  },

  findAll: function(params) {
    return this.findInternal_(params, []);
  },

  matches: function(params) {
    return this.matchInternal_(params);
  },

  getNextTextMatch: function(searchStr, backward) {
    var info = GetNextTextMatch(this.treeID, this.id, searchStr, backward);

    if (!info)
      return;

    var impl = privates(AutomationRootNodeImpl.get(info.treeId)).impl;
    if (impl)
      return impl.get(info.nodeId);
  },

  addEventListener: function(eventType, callback, capture) {
    this.removeEventListener(eventType, callback);
    if (!this.listeners[eventType])
      this.listeners[eventType] = [];
    $Array.push(this.listeners[eventType], {
      __proto__: null,
      callback: callback,
      capture: !!capture,
    });
  },

  // TODO(dtseng/aboxhall): Check this impl against spec.
  removeEventListener: function(eventType, callback) {
    if (this.listeners[eventType]) {
      var listeners = this.listeners[eventType];
      for (var i = 0; i < listeners.length; i++) {
        if (callback === listeners[i].callback)
          $Array.splice(listeners, i, 1);
      }
    }
  },

  toJSON: function() {
    return { treeID: this.treeID,
             id: this.id,
             role: this.role,
             attributes: this.attributes };
  },

  dispatchEvent: function(eventType, eventFrom, mouseX, mouseY) {
    var path = [];
    var parent = this.parent;
    while (parent) {
      $Array.push(path, parent);
      parent = parent.parent;
    }
    var event = new AutomationEvent(eventType, this.wrapper, eventFrom);
    event.mouseX = mouseX;
    event.mouseY = mouseY;

    // Dispatch the event through the propagation path in three phases:
    // - capturing: starting from the root and going down to the target's parent
    // - targeting: dispatching the event on the target itself
    // - bubbling: starting from the target's parent, going back up to the root.
    // At any stage, a listener may call stopPropagation() on the event, which
    // will immediately stop event propagation through this path.
    if (this.dispatchEventAtCapturing_(event, path)) {
      if (this.dispatchEventAtTargeting_(event, path))
        this.dispatchEventAtBubbling_(event, path);
    }
  },

  toString: function() {
    var parentID = GetParentID(this.treeID, this.id);
    parentID = parentID ? parentID.nodeId : null;
    var childTreeID = GetStringAttribute(this.treeID, this.id, 'childTreeId');
    var count = GetChildCount(this.treeID, this.id);
    var childIDs = [];
    for (var i = 0; i < count; ++i) {
      var childID = GetChildIDAtIndex(this.treeID, this.id, i).nodeId;
      $Array.push(childIDs, childID);
    }
    var name = GetStringAttribute(this.treeID, this.id, 'name');

    var result = 'node id=' + this.id +
        ' role=' + this.role +
        ' state=' + $JSON.stringify(this.state) +
        ' parentID=' + parentID +
        ' childIds=' + $JSON.stringify(childIDs);
    if (childTreeID)
      result += ' childTreeID=' + childTreeID;
    if (name)
      result += ' name=' + name;
    return result;
  },

  dispatchEventAtCapturing_: function(event, path) {
    privates(event).impl.eventPhase = Event.CAPTURING_PHASE;
    for (var i = path.length - 1; i >= 0; i--) {
      this.fireEventListeners_(path[i], event);
      if (privates(event).impl.propagationStopped)
        return false;
    }
    return true;
  },

  dispatchEventAtTargeting_: function(event) {
    privates(event).impl.eventPhase = Event.AT_TARGET;
    this.fireEventListeners_(this.wrapper, event);
    return !privates(event).impl.propagationStopped;
  },

  dispatchEventAtBubbling_: function(event, path) {
    privates(event).impl.eventPhase = Event.BUBBLING_PHASE;
    for (var i = 0; i < path.length; i++) {
      this.fireEventListeners_(path[i], event);
      if (privates(event).impl.propagationStopped)
        return false;
    }
    return true;
  },

  fireEventListeners_: function(node, event) {
    var nodeImpl = privates(node).impl;
    if (!nodeImpl.rootImpl)
      return;

    var listeners = nodeImpl.listeners[event.type];
    if (!listeners)
      return;
    var eventPhase = event.eventPhase;
    for (var i = 0; i < listeners.length; i++) {
      if (eventPhase == Event.CAPTURING_PHASE && !listeners[i].capture)
        continue;
      if (eventPhase == Event.BUBBLING_PHASE && listeners[i].capture)
        continue;

      try {
        listeners[i].callback(event);
      } catch (e) {
        exceptionHandler.handle('Error in event handler for ' + event.type +
            ' during phase ' + eventPhase, e);
      }
    }
  },

  performAction_: function(actionType, opt_args, opt_callback) {
    if (!this.rootImpl)
      return;

    // Not yet initialized.
    if (this.rootImpl.treeID === undefined ||
        this.id === undefined) {
      return;
    }

    // Check permissions.
    if (!IsInteractPermitted()) {
      throw new Error(actionType + ' requires {"desktop": true} or' +
          ' {"interact": true} in the "automation" manifest key.');
    }
    var requestID = -1;
    if (opt_callback) {
      requestID = this.rootImpl.addActionResultCallback(opt_callback);
    }

    automationInternal.performAction({ treeID: this.rootImpl.treeID,
                                       automationNodeID: this.id,
                                       actionType: actionType,
                                       requestID: requestID},
                                     opt_args || {});
  },

  domQuerySelectorCallback_: function(userCallback, resultAutomationNodeID) {
    // resultAutomationNodeID could be zero or undefined or (unlikely) null;
    // they all amount to the same thing here, which is that no node was
    // returned.
    if (!resultAutomationNodeID || !this.rootImpl) {
      userCallback(null);
      return;
    }
    var resultNode = this.rootImpl.get(resultAutomationNodeID);
    if (!resultNode) {
      logging.WARNING('Query selector result not in tree: ' +
                      resultAutomationNodeID);
      userCallback(null);
    }
    userCallback(resultNode);
  },

  findInternal_: function(params, opt_results) {
    var result = null;
    this.forAllDescendants_(function(node) {
      if (privates(node).impl.matchInternal_(params)) {
        if (opt_results)
          $Array.push(opt_results, node);
        else
          result = node;
        return !opt_results;
      }
    });
    if (opt_results)
      return opt_results;
    return result;
  },

  /**
   * Executes a closure for all of this node's descendants, in pre-order.
   * Early-outs if the closure returns true.
   * @param {Function(AutomationNode):boolean} closure Closure to be executed
   *     for each node. Return true to early-out the traversal.
   */
  forAllDescendants_: function(closure) {
    var stack = $Array.reverse(this.wrapper.children);
    while (stack.length > 0) {
      var node = $Array.pop(stack);
      if (closure(node))
        return;

      var children = node.children;
      for (var i = children.length - 1; i >= 0; i--)
        $Array.push(stack, children[i]);
    }
  },

  matchInternal_: function(params) {
    if ($Object.keys(params).length === 0)
      return false;

    if ('role' in params && this.role != params.role)
        return false;

    if ('state' in params) {
      for (var state in params.state) {
        if (params.state[state] != (state in this.state))
          return false;
      }
    }
    if ('attributes' in params) {
      for (var attribute in params.attributes) {
        var attrValue = params.attributes[attribute];
        if (typeof attrValue != 'object') {
          if (this[attribute] !== attrValue)
            return false;
        } else if (attrValue instanceof $RegExp.self) {
          if (typeof this[attribute] != 'string')
            return false;
          if (!attrValue.test(this[attribute]))
            return false;
        } else {
          // TODO(aboxhall): handle intlist case.
          return false;
        }
      }
    }
    return true;
  }
};

var stringAttributes = [
    'accessKey',
    'ariaInvalidValue',
    'autoComplete',
    'className',
    'containerLiveRelevant',
    'containerLiveStatus',
    'description',
    'display',
    'htmlTag',
    'imageDataUrl',
    'innerHtml',
    'language',
    'liveRelevant',
    'liveStatus',
    'name',
    'placeholder',
    'roleDescription',
    'textInputType',
    'url',
    'value'];

var boolAttributes = [
    'busy',
    'clickable',
    'containerLiveAtomic',
    'containerLiveBusy',
    'liveAtomic',
    'modal',
    'scrollable',
    'selected'
];

var intAttributes = [
    'backgroundColor',
    'color',
    'colorValue',
    'hierarchicalLevel',
    'posInSet',
    'scrollX',
    'scrollXMax',
    'scrollXMin',
    'scrollY',
    'scrollYMax',
    'scrollYMin',
    'setSize',
    'tableCellColumnIndex',
    'ariaCellColumnIndex',
    'tableCellColumnSpan',
    'tableCellRowIndex',
    'ariaCellRowIndex',
    'tableCellRowSpan',
    'tableColumnCount',
    'ariaColumnCount',
    'tableColumnIndex',
    'tableRowCount',
    'ariaRowCount',
    'tableRowIndex',
    'textSelEnd',
    'textSelStart'];

// Int attribute, relation property to expose, reverse relation to expose.
var nodeRefAttributes = [
    ['activedescendantId', 'activeDescendant', 'activeDescendantFor'],
    ['detailsId', 'details', 'detailsFor'],
    ['errorMessageId', 'errorMessage', 'errorMessageFor'],
    ['inPageLinkTargetId', 'inPageLinkTarget', null],
    ['nextFocusId', 'nextFocus', null],
    ['nextOnLineId', 'nextOnLine', null],
    ['previousFocusId', 'previousFocus', null],
    ['previousOnLineId', 'previousOnLine', null],
    ['tableColumnHeaderId', 'tableColumnHeader', null],
    ['tableHeaderId', 'tableHeader', null],
    ['tableRowHeaderId', 'tableRowHeader', null]];

var intListAttributes = [
    'lineBreaks',
    'markerEnds',
    'markerStarts',
    'markerTypes',
    'wordEnds',
    'wordStarts'];

// Intlist attribute, relation property to expose, reverse relation to expose.
var nodeRefListAttributes = [
    ['controlsIds', 'controls', 'controlledBy'],
    ['describedbyIds', 'describedBy', 'descriptionFor'],
    ['flowtoIds', 'flowTo', 'flowFrom'],
    ['labelledbyIds', 'labelledBy', 'labelFor']];

var floatAttributes = [
    'valueForRange',
    'minValueForRange',
    'maxValueForRange'];

var htmlAttributes = [
    ['type', 'inputType']];

var publicAttributes = [];

$Array.forEach(stringAttributes, function(attributeName) {
  $Array.push(publicAttributes, attributeName);
  $Object.defineProperty(AutomationNodeImpl.prototype, attributeName, {
    __proto__: null,
    get: function() {
      return GetStringAttribute(this.treeID, this.id, attributeName);
    }
  });
});

$Array.forEach(boolAttributes, function(attributeName) {
  $Array.push(publicAttributes, attributeName);
  $Object.defineProperty(AutomationNodeImpl.prototype, attributeName, {
    __proto__: null,
    get: function() {
      return GetBoolAttribute(this.treeID, this.id, attributeName);
    }
  });
});

$Array.forEach(intAttributes, function(attributeName) {
  $Array.push(publicAttributes, attributeName);
  $Object.defineProperty(AutomationNodeImpl.prototype, attributeName, {
    __proto__: null,
    get: function() {
      return GetIntAttribute(this.treeID, this.id, attributeName);
    }
  });
});

$Array.forEach(nodeRefAttributes, function(params) {
  var srcAttributeName = params[0];
  var dstAttributeName = params[1];
  var dstReverseAttributeName = params[2];
  $Array.push(publicAttributes, dstAttributeName);
  $Object.defineProperty(AutomationNodeImpl.prototype, dstAttributeName, {
    __proto__: null,
    get: function() {
      var id = GetIntAttribute(this.treeID, this.id, srcAttributeName);
      if (id && this.rootImpl)
        return this.rootImpl.get(id);
      else
        return undefined;
    }
  });
  if (dstReverseAttributeName) {
    $Array.push(publicAttributes, dstReverseAttributeName);
    $Object.defineProperty(AutomationNodeImpl.prototype,
                           dstReverseAttributeName, {
      __proto__: null,
      get: function() {
        var ids = GetIntAttributeReverseRelations(
            this.treeID, this.id, srcAttributeName);
        if (!ids || !this.rootImpl)
          return undefined;
        var result = [];
        for (var i = 0; i < ids.length; ++i) {
          var node = this.rootImpl.get(ids[i]);
          if (node)
          $Array.push(result, node);
        }
        return result;
      }
    });
  }
});

$Array.forEach(intListAttributes, function(attributeName) {
  $Array.push(publicAttributes, attributeName);
  $Object.defineProperty(AutomationNodeImpl.prototype, attributeName, {
    __proto__: null,
    get: function() {
      return GetIntListAttribute(this.treeID, this.id, attributeName);
    }
  });
});

$Array.forEach(nodeRefListAttributes, function(params) {
  var srcAttributeName = params[0];
  var dstAttributeName = params[1];
  var dstReverseAttributeName = params[2];
  $Array.push(publicAttributes, dstAttributeName);
  $Object.defineProperty(AutomationNodeImpl.prototype, dstAttributeName, {
    __proto__: null,
    get: function() {
      var ids = GetIntListAttribute(this.treeID, this.id, srcAttributeName);
      if (!ids || !this.rootImpl)
        return undefined;
      var result = [];
      for (var i = 0; i < ids.length; ++i) {
        var node = this.rootImpl.get(ids[i]);
        if (node)
          $Array.push(result, node);
      }
      return result;
    }
  });
  if (dstReverseAttributeName) {
    $Array.push(publicAttributes, dstReverseAttributeName);
    $Object.defineProperty(AutomationNodeImpl.prototype,
                           dstReverseAttributeName, {
      __proto__: null,
      get: function() {
        var ids = GetIntListAttributeReverseRelations(
            this.treeID, this.id, srcAttributeName);
        if (!ids || !this.rootImpl)
          return undefined;
        var result = [];
        for (var i = 0; i < ids.length; ++i) {
          var node = this.rootImpl.get(ids[i]);
          if (node)
          $Array.push(result, node);
        }
        return result;
      }
    });
  }
});

$Array.forEach(floatAttributes, function(attributeName) {
  $Array.push(publicAttributes, attributeName);
  $Object.defineProperty(AutomationNodeImpl.prototype, attributeName, {
    __proto__: null,
    get: function() {
      return GetFloatAttribute(this.treeID, this.id, attributeName);
    }
  });
});

$Array.forEach(htmlAttributes, function(params) {
  var srcAttributeName = params[0];
  var dstAttributeName = params[1];
  $Array.push(publicAttributes, dstAttributeName);
  $Object.defineProperty(AutomationNodeImpl.prototype, dstAttributeName, {
    __proto__: null,
    get: function() {
      return GetHtmlAttribute(this.treeID, this.id, srcAttributeName);
    }
  });
});

/**
 * AutomationRootNode.
 *
 * An AutomationRootNode is the javascript end of an AXTree living in the
 * browser. AutomationRootNode handles unserializing incremental updates from
 * the source AXTree. Each update contains node data that form a complete tree
 * after applying the update.
 *
 * A brief note about ids used through this class. The source AXTree assigns
 * unique ids per node and we use these ids to build a hash to the actual
 * AutomationNode object.
 * Thus, tree traversals amount to a lookup in our hash.
 *
 * The tree itself is identified by the accessibility tree id of the
 * renderer widget host.
 * @constructor
 */
function AutomationRootNodeImpl(treeID) {
  $Function.call(AutomationNodeImpl, this, this);
  this.treeID = treeID;
  this.axNodeDataCache_ = {__proto__: null};
}

utils.defineProperty(AutomationRootNodeImpl, 'idToAutomationRootNode_',
    {__proto__: null});

utils.defineProperty(AutomationRootNodeImpl, 'get', function(treeID) {
  var result = AutomationRootNodeImpl.idToAutomationRootNode_[treeID];
  return result || undefined;
});

utils.defineProperty(AutomationRootNodeImpl, 'getOrCreate', function(treeID) {
  if (AutomationRootNodeImpl.idToAutomationRootNode_[treeID])
    return AutomationRootNodeImpl.idToAutomationRootNode_[treeID];
  var result = new AutomationRootNode(treeID);
  AutomationRootNodeImpl.idToAutomationRootNode_[treeID] = result;
  return result;
});

utils.defineProperty(
    AutomationRootNodeImpl, 'getNodeFromTree', function(treeId, nodeId) {
  var impl = privates(AutomationRootNodeImpl.get(treeId)).impl;
  if (impl)
    return impl.get(nodeId);
});

utils.defineProperty(AutomationRootNodeImpl, 'destroy', function(treeID) {
  delete AutomationRootNodeImpl.idToAutomationRootNode_[treeID];
});

/**
 * A counter keeping track of IDs to use for mapping action requests to
 * their callback function.
 */
AutomationRootNodeImpl.actionRequestCounter = 0;

/**
 * A map from a request ID to the corresponding callback function to call
 * when the action response event is received.
 */
AutomationRootNodeImpl.actionRequestIDToCallback = {};

AutomationRootNodeImpl.prototype = {
  __proto__: AutomationNodeImpl.prototype,

  /**
   * @type {boolean}
   */
  isRootNode: true,

  /**
   * @type {string}
   */
  treeID: "",

  /**
   * A map from id to AutomationNode.
   * @type {Object.<number, AutomationNode>}
   * @private
   */
  axNodeDataCache_: null,

  get id() {
    var result = GetRootID(this.treeID);

    // Don't return undefined, because the id is often passed directly
    // as an argument to a native binding that expects only a valid number.
    if (result === undefined)
      return -1;

    return result;
  },

  get docUrl() {
    return GetDocURL(this.treeID);
  },

  get docTitle() {
    return GetDocTitle(this.treeID);
  },

  get docLoaded() {
    return GetDocLoaded(this.treeID);
  },

  get docLoadingProgress() {
    return GetDocLoadingProgress(this.treeID);
  },

  get anchorObject() {
    var id = GetAnchorObjectID(this.treeID);
    if (id && id != -1)
      return this.get(id);
    else
      return undefined;
  },

  get anchorOffset() {
    var id = GetAnchorObjectID(this.treeID);
    if (id && id != -1)
      return GetAnchorOffset(this.treeID);
  },

  get anchorAffinity() {
    var id = GetAnchorObjectID(this.treeID);
    if (id && id != -1)
      return GetAnchorAffinity(this.treeID);
  },

  get focusObject() {
    var id = GetFocusObjectID(this.treeID);
    if (id && id != -1)
      return this.get(id);
    else
      return undefined;
  },

  get focusOffset() {
    var id = GetFocusObjectID(this.treeID);
    if (id && id != -1)
      return GetFocusOffset(this.treeID);
  },

  get focusAffinity() {
    var id = GetFocusObjectID(this.treeID);
    if (id && id != -1)
      return GetFocusAffinity(this.treeID);
  },

  get: function(id) {
    if (id == undefined)
      return undefined;

    if (id == this.id)
      return this.wrapper;

   var obj = this.axNodeDataCache_[id];
    if (obj)
      return obj;

    // Validate the backing AXTree has the specified node.
    if (!GetRole(this.treeID, id))
      return;

    obj = new AutomationNode(this);
    privates(obj).impl.treeID = this.treeID;
    privates(obj).impl.id = id;
    this.axNodeDataCache_[id] = obj;

    return obj;
  },

  remove: function(id) {
    if (this.axNodeDataCache_[id])
      privates(this.axNodeDataCache_[id]).impl.detach();
    delete this.axNodeDataCache_[id];
  },

  destroy: function() {
    this.dispatchEvent('destroyed', 'none');
    for (var id in this.axNodeDataCache_)
      this.remove(id);
    this.detach();
  },

  onAccessibilityEvent: function(eventParams) {
    var targetNode = this.get(eventParams.targetID);
    if (targetNode) {
      var targetNodeImpl = privates(targetNode).impl;
      targetNodeImpl.dispatchEvent(
          eventParams.eventType, eventParams.eventFrom,
          eventParams.mouseX, eventParams.mouseY);

      if (eventParams.actionRequestID != -1) {
        this.onActionResult(eventParams.actionRequestID, targetNode);
      }
    } else {
      logging.WARNING('Got ' + eventParams.eventType +
                      ' event on unknown node: ' + eventParams.targetID +
                      '; this: ' + this.id);
    }
    return true;
  },

  addActionResultCallback: function(callback) {
    AutomationRootNodeImpl.actionRequestIDToCallback[
        ++AutomationRootNodeImpl.actionRequestCounter] = callback;
    return AutomationRootNodeImpl.actionRequestCounter;
  },

  onActionResult: function(requestID, result) {
    if (requestID in AutomationRootNodeImpl.actionRequestIDToCallback) {
      AutomationRootNodeImpl.actionRequestIDToCallback[requestID](result);
      delete AutomationRootNodeImpl.actionRequestIDToCallback[requestID];
    }
  },

  toString: function() {
    function toStringInternal(nodeImpl, indent) {
      if (!nodeImpl)
        return '';
      var output = '';
      if (nodeImpl.isRootNode)
        output += indent + 'tree id=' + nodeImpl.treeID + '\n';
      output += indent +
        $Function.call(AutomationNodeImpl.prototype.toString, nodeImpl) + '\n';
      indent += '  ';
      var children = nodeImpl.children;
      for (var i = 0; i < children.length; ++i)
        output += toStringInternal(privates(children[i]).impl, indent);
      return output;
    }
    return toStringInternal(this, '');
  },
};

function AutomationNode() {
  privates(AutomationNode).constructPrivate(this, arguments);
}
utils.expose(AutomationNode, AutomationNodeImpl, {
  functions: [
    'doDefault',
    'find',
    'findAll',
    'focus',
    'getImageData',
    'getNextTextMatch',
    'hitTest',
    'hitTestWithReply',
    'makeVisible',
    'matches',
    'performCustomAction',
    'performStandardAction',
    'replaceSelectedText',
    'resumeMedia',
    'scrollBackward',
    'scrollForward',
    'scrollUp',
    'scrollDown',
    'scrollLeft',
    'scrollRight',
    'setSelection',
    'setSequentialFocusNavigationStartingPoint',
    'setValue',
    'showContextMenu',
    'startDuckingMedia',
    'stopDuckingMedia',
    'suspendMedia',
    'addEventListener',
    'removeEventListener',
    'domQuerySelector',
    'toString',
    'boundsForRange',
  ],
  readonly: $Array.concat(publicAttributes, [
      'parent',
      'firstChild',
      'lastChild',
      'children',
      'previousSibling',
      'nextSibling',
      'isRootNode',
      'role',
      'checked',
      'defaultActionVerb',
      'restriction',
      'state',
      'location',
      'indexInParent',
      'lineStartOffsets',
      'root',
      'htmlAttributes',
      'nameFrom',
      'bold',
      'italic',
      'underline',
      'lineThrough',
      'customActions',
      'standardActions',
      'unclippedLocation',
      'tableCellColumnHeaders',
      'tableCellRowHeaders',
  ]),
});

function AutomationRootNode() {
  privates(AutomationRootNode).constructPrivate(this, arguments);
}
utils.expose(AutomationRootNode, AutomationRootNodeImpl, {
  superclass: AutomationNode,
  readonly: [
    'docTitle',
    'docUrl',
    'docLoaded',
    'docLoadingProgress',
    'anchorObject',
    'anchorOffset',
    'anchorAffinity',
    'focusObject',
    'focusOffset',
    'focusAffinity',
  ],
});

utils.defineProperty(AutomationRootNode, 'get', function(treeID) {
  return AutomationRootNodeImpl.get(treeID);
});

utils.defineProperty(AutomationRootNode, 'getOrCreate', function(treeID) {
  return AutomationRootNodeImpl.getOrCreate(treeID);
});

utils.defineProperty(AutomationRootNode, 'destroy', function(treeID) {
  AutomationRootNodeImpl.destroy(treeID);
});

exports.$set('AutomationNode', AutomationNode);
exports.$set('AutomationRootNode', AutomationRootNode);
