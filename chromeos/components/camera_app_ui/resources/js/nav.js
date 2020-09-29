// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxy} from './browser_proxy/browser_proxy.js';
import {assertInstanceof} from './chrome_util.js';
import * as dom from './dom.js';
import {DeviceOperator} from './mojo/device_operator.js';
import * as state from './state.js';
import * as toast from './toast.js';
// eslint-disable-next-line no-unused-vars
import {ViewName} from './type.js';
import * as util from './util.js';
// eslint-disable-next-line no-unused-vars
import {View} from './views/view.js';

/**
 * All views stacked in ascending z-order (DOM order) for navigation, and only
 * the topmost visible view is active (clickable/focusable).
 * @type {!Array<!View>}
 */
let allViews = [];

/**
 * Index of the current topmost visible view in the stacked views.
 * @type {number}
 */
let topmostIndex = -1;

/**
 * Sets up navigation for all views, e.g. camera-view, dialog-view, etc.
 * @param {!Array<!View>} views All views in ascending z-order.
 */
export function setup(views) {
  allViews = views;
  // Manage all tabindex usages in for navigation.
  dom.getAll('[tabindex]', HTMLElement)
      .forEach((element) => util.makeUnfocusableByMouse(element));
  document.body.addEventListener('keydown', (event) => {
    const e = assertInstanceof(event, KeyboardEvent);
    if (e.key === 'Tab') {
      state.set(state.State.TAB_NAVIGATION, true);
    }
  });
  document.body.addEventListener(
      'pointerdown', () => state.set(state.State.TAB_NAVIGATION, false));
}

/**
 * Activates the view to be focusable.
 * @param {number} index Index of the view.
 */
function activate(index) {
  // Restore the view's child elements' tabindex and then focus the view.
  const view = allViews[index];
  view.root.setAttribute('aria-hidden', 'false');
  dom.getAllFrom(view.root, '[tabindex]', HTMLElement).forEach((element) => {
    element.setAttribute('tabindex', element.dataset['tabindex']);
    element.removeAttribute('data-tabindex');
  });
  view.focus();
}

/**
 * Inactivates the view to be unfocusable.
 * @param {number} index Index of the view.
 */
function inactivate(index) {
  const view = allViews[index];
  view.root.setAttribute('aria-hidden', 'true');
  dom.getAllFrom(view.root, '[tabindex]', HTMLElement).forEach((element) => {
    element.dataset['tabindex'] = element.getAttribute('tabindex');
    element.setAttribute('tabindex', '-1');
  });
  document.activeElement.blur();
}

/**
 * Checks if the view is already shown.
 * @param {number} index Index of the view.
 * @return {boolean} Whether the view is shown or not.
 */
function isShown(index) {
  return state.get(allViews[index].name);
}

/**
 * Shows the view indexed in the stacked views and activates the view only if
 * it becomes the topmost visible view.
 * @param {number} index Index of the view.
 * @return {!View} View shown.
 */
function show(index) {
  const view = allViews[index];
  if (!isShown(index)) {
    state.set(view.name, true);
    view.layout();
    if (index > topmostIndex) {
      if (topmostIndex >= 0) {
        inactivate(topmostIndex);
      }
      activate(index);
      topmostIndex = index;
    }
  }
  return view;
}

/**
 * Finds the next topmost visible view in the stacked views.
 * @return {number} Index of the view found; otherwise, -1.
 */
function findNextTopmostIndex() {
  for (let i = topmostIndex - 1; i >= 0; i--) {
    if (isShown(i)) {
      return i;
    }
  }
  return -1;
}

/**
 * Hides the view indexed in the stacked views and inactivate the view if it was
 * the topmost visible view.
 * @param {number} index Index of the view.
 */
function hide(index) {
  if (index === topmostIndex) {
    inactivate(index);
    const next = findNextTopmostIndex();
    if (next >= 0) {
      activate(next);
    }
    topmostIndex = next;
  }
  state.set(allViews[index].name, false);
}

/**
 * Finds the view by its name in the stacked views.
 * @param {!ViewName} name View name.
 * @return {number} Index of the view found; otherwise, -1.
 */
function findIndex(name) {
  return allViews.findIndex((view) => view.name === name);
}

/**
 * Opens a navigation session of the view; shows the view before entering it and
 * hides the view after leaving it for the ended session.
 * @param {!ViewName} name View name.
 * @param {...*} args Optional rest parameters for entering the view.
 * @return {!Promise<*>} Promise for the operation or result.
 */
export function open(name, ...args) {
  const index = findIndex(name);
  return show(index).enter(...args).finally(() => {
    hide(index);
  });
}

/**
 * Closes the current navigation session of the view by leaving it.
 * @param {!ViewName} name View name.
 * @param {*=} condition Optional condition for leaving the view.
 * @return {boolean} Whether successfully leaving the view or not.
 */
export function close(name, condition) {
  const index = findIndex(name);
  return allViews[index].leave(condition);
}

/**
 * Handles key pressed event.
 * @param {!KeyboardEvent} event Key press event.
 */
export function onKeyPressed(event) {
  const key = util.getShortcutIdentifier(event);
  switch (key) {
    case 'BrowserBack':
      chrome.app.window.current().minimize();
      break;
    case 'Ctrl-V':
      toast.showDebugMessage(browserProxy.getAppVersion());
      break;
    case 'Ctrl-Shift-I':
      browserProxy.openInspector('normal');
      break;
    case 'Ctrl-Shift-J':
      browserProxy.openInspector('console');
      break;
    case 'Ctrl-Shift-C':
      browserProxy.openInspector('element');
      break;
    case 'Ctrl-Shift-E':
      (async () => {
        if (!await DeviceOperator.isSupported()) {
          toast.show('error_msg_expert_mode_not_supported');
          return;
        }
        const newState = !state.get(state.State.EXPERT);
        state.set(state.State.EXPERT, newState);
        browserProxy.localStorageSet({expert: newState});
      })();
      break;
    default:
      // Make the topmost visible view handle the pressed key.
      if (topmostIndex >= 0 && allViews[topmostIndex].onKeyPressed(key)) {
        event.preventDefault();
      }
  }
}

/**
 * Handles resized window on current all visible views.
 */
export function onWindowResized() {
  // All visible views need being relayout after window is resized.
  for (let i = allViews.length - 1; i >= 0; i--) {
    if (isShown(i)) {
      allViews[i].layout();
    }
  }
}
