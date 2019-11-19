// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Common utilities for extension ui tests. */
import {MockController, MockMethod} from '../mock_controller.m.js';
import {isVisible} from '../test_util.m.js';

import {TestKioskBrowserProxy} from './test_kiosk_browser_proxy.js';

/** A mock to test that clicking on an element calls a specific method. */
export class ClickMock {
  /**
   * Tests clicking on an element and expecting a call.
   * @param {HTMLElement} element The element to click on.
   * @param {string} callName The function expected to be called.
   * @param {Array<*>=} opt_expectedArgs The arguments the function is
   *     expected to be called with.
   * @param {*=} opt_returnValue The value to return from the function call.
   */
  testClickingCalls(element, callName, opt_expectedArgs, opt_returnValue) {
    const mock = new MockController();
    const mockMethod = mock.createFunctionMock(this, callName);
    mockMethod.returnValue = opt_returnValue;
    MockMethod.prototype.addExpectation.apply(mockMethod, opt_expectedArgs);
    element.click();
    mock.verifyMocks();
  }
}

/**
 * A mock to test receiving expected events and verify that they were called
 * with the proper detail values.
 */
export class ListenerMock {
  constructor() {
    /** @private {Object<{satisfied: boolean, args: !Object}>} */
    this.listeners_ = {};
  }

  /**
   * @param {string} eventName
   * @param {Event} e
   */
  onEvent_(eventName, e) {
    assert(this.listeners_.hasOwnProperty(eventName));
    if (this.listeners_[eventName].satisfied) {
      // Event was already called and checked. We could always make this
      // more intelligent by allowing for subsequent calls, removing the
      // listener, etc, but there's no need right now.
      return;
    }
    const expected = this.listeners_[eventName].args || {};
    expectDeepEquals(e.detail, expected);
    this.listeners_[eventName].satisfied = true;
  }

  /**
   * Adds an expected event.
   * @param {!EventTarget} target
   * @param {string} eventName
   * @param {Object=} opt_eventArgs If omitted, will check that the details
   *     are empty (i.e., {}).
   */
  addListener(target, eventName, opt_eventArgs) {
    assert(!this.listeners_.hasOwnProperty(eventName));
    this.listeners_[eventName] = {args: opt_eventArgs || {}, satisfied: false};
    target.addEventListener(eventName, this.onEvent_.bind(this, eventName));
  }

  /** Verifies the expectations set. */
  verify() {
    const missingEvents = [];
    for (const key in this.listeners_) {
      if (!this.listeners_[key].satisfied) {
        missingEvents.push(key);
      }
    }
    expectEquals(0, missingEvents.length, JSON.stringify(missingEvents));
  }
}

/**
 * A mock delegate for the item, capable of testing functionality.
 * @implements {extensions.ItemDelegate}
 */
export class MockItemDelegate extends ClickMock {
  constructor() {
    super();
  }

  /** @override */
  deleteItem(id) {}

  /** @override */
  setItemEnabled(id, enabled) {}

  /** @override */
  showItemDetails(id) {}

  /** @override */
  setItemAllowedIncognito(id, enabled) {}

  /** @override */
  setItemAllowedOnFileUrls(id, enabled) {}

  /** @override */
  setItemHostAccess(id, hostAccess) {}

  /** @override */
  setItemCollectsErrors(id, enabled) {}

  /** @override */
  inspectItemView(id, view) {}

  /** @override */
  reloadItem(id) {}

  /** @override */
  repairItem(id) {}

  /** @override */
  showItemOptionsPage(id) {}

  /** @override */
  showInFolder(id) {}

  /** @override */
  getExtensionSize(id) {
    return Promise.resolve('10 MB');
  }
}

/**
 * @param {!HTMLElement} element
 * @return {boolean} whether or not the element passed in is visible
 */
export function isElementVisible(element) {
  const rect = element.getBoundingClientRect();
  return rect.width * rect.height > 0;  // Width and height is never negative.
}

/**
 * Tests that the element's visibility matches |expectedVisible| and,
 * optionally, has specific content if it is visible.
 * @param {!HTMLElement} parentEl The parent element to query for the element.
 * @param {string} selector The selector to find the element.
 * @param {boolean} expectedVisible Whether the element should be
 *     visible.
 * @param {string=} opt_expectedText The expected textContent value.
 */
export function testVisible(
    parentEl, selector, expectedVisible, opt_expectedText) {
  const visible = isVisible(parentEl, selector);
  expectEquals(expectedVisible, visible, selector);
  if (expectedVisible && visible && opt_expectedText) {
    const element = parentEl.$$(selector);
    expectEquals(opt_expectedText, element.textContent.trim(), selector);
  }
}

/**
 * Creates an ExtensionInfo object.
 * @param {Object=} opt_properties A set of properties that will be used on
 *     the resulting ExtensionInfo (otherwise defaults will be used).
 * @return {chrome.developerPrivate.ExtensionInfo}
 */
export function createExtensionInfo(opt_properties) {
  const id = opt_properties && opt_properties.hasOwnProperty('id') ?
      opt_properties['id'] :
      'a'.repeat(32);
  const baseUrl = 'chrome-extension://' + id + '/';
  return Object.assign(
      {
        commands: [],
        dependentExtensions: [],
        description: 'This is an extension',
        disableReasons: {
          suspiciousInstall: false,
          corruptInstall: false,
          updateRequired: false,
        },
        homePage: {specified: false, url: ''},
        iconUrl: 'chrome://extension-icon/' + id + '/24/0',
        id: id,
        incognitoAccess: {isEnabled: true, isActive: false},
        location: 'FROM_STORE',
        manifestErrors: [],
        name: 'Wonderful Extension',
        runtimeErrors: [],
        runtimeWarnings: [],
        permissions: {simplePermissions: []},
        state: 'ENABLED',
        type: 'EXTENSION',
        userMayModify: true,
        version: '2.0',
        views: [{url: baseUrl + 'foo.html'}, {url: baseUrl + 'bar.html'}],
      },
      opt_properties);
}

/**
 * Finds all nodes matching |query| under |root|, within self and children's
 * Shadow DOM.
 * @param {!Node} root
 * @param {string} query The CSS query
 * @return {!Array<!HTMLElement>}
 */
export function findMatches(root, query) {
  let elements = new Set();
  function doSearch(node) {
    if (node.nodeType == Node.ELEMENT_NODE) {
      const matches = node.querySelectorAll(query);
      for (let match of matches) {
        elements.add(match);
      }
    }
    let child = node.firstChild;
    while (child !== null) {
      doSearch(child);
      child = child.nextSibling;
    }
    const shadowRoot = node.shadowRoot;
    if (shadowRoot) {
      doSearch(shadowRoot);
    }
  }
  doSearch(root);
  return Array.from(elements);
}
