// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Common utilities for extension ui tests. */
import type {ItemDelegate} from 'chrome://extensions/extensions.js';
import {createDummyExtensionInfo} from 'chrome://extensions/extensions.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeChromeEvent} from 'chrome://webui-test/fake_chrome_event.js';
import {MockController, MockMethod} from 'chrome://webui-test/mock_controller.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';

/** A mock to test that clicking on an element calls a specific method. */
export class ClickMock {
  /**
   * Tests clicking on an element and expecting a call.
   * @param element The element to click on.
   * @param callName The function expected to be called.
   * @param expectedArgs The arguments the function is
   *     expected to be called with.
   * @param returnValue The value to return from the function call.
   */
  async testClickingCalls(
      element: HTMLElement, callName: string, expectedArgs: any[],
      returnValue?: any) {
    const mock = new MockController();
    const mockMethod = mock.createFunctionMock(this, callName);
    mockMethod.returnValue = returnValue;
    MockMethod.prototype.addExpectation.apply(mockMethod, expectedArgs);
    element.click();

    if (element instanceof CrLitElement) {
      await (element as CrLitElement).updateComplete;
    }

    mock.verifyMocks();
  }
}

interface ListenerInfo {
  satisfied: boolean;
  args: any;
}

/**
 * A mock to test receiving expected events and verify that they were called
 * with the proper detail values.
 */
export class ListenerMock {
  private listeners_: {[eventName: string]: ListenerInfo} = {};

  private onEvent_(eventName: string, e: Event) {
    assertTrue(this.listeners_.hasOwnProperty(eventName));
    if (this.listeners_[eventName]!.satisfied) {
      // Event was already called and checked. We could always make this
      // more intelligent by allowing for subsequent calls, removing the
      // listener, etc, but there's no need right now.
      return;
    }
    const expected = this.listeners_[eventName]!.args || {};
    assertDeepEquals((e as CustomEvent).detail, expected);
    this.listeners_[eventName]!.satisfied = true;
  }

  /**
   * Adds an expected event.
   * @param eventArgs If omitted, will check that the details
   *     are empty (i.e., {}).
   */
  addListener(target: EventTarget, eventName: string, eventArgs: any) {
    assertTrue(!this.listeners_.hasOwnProperty(eventName));
    this.listeners_[eventName] = {args: eventArgs || {}, satisfied: false};
    target.addEventListener(eventName, this.onEvent_.bind(this, eventName));
  }

  /** Verifies the expectations set. */
  verify() {
    const missingEvents = [];
    for (const key in this.listeners_) {
      if (!this.listeners_[key]!.satisfied) {
        missingEvents.push(key);
      }
    }
    assertEquals(0, missingEvents.length, JSON.stringify(missingEvents));
  }
}

/**
 * A mock delegate for the item, capable of testing functionality.
 */
export class MockItemDelegate extends ClickMock implements ItemDelegate {
  itemStateChangedTarget: FakeChromeEvent = new FakeChromeEvent();
  deleteItem(_id: string) {}
  deleteItems(_ids: string[]) {
    return Promise.resolve();
  }
  uninstallItem(_id: string) {
    return Promise.resolve();
  }
  setItemSafetyCheckWarningAcknowledged(_id: string) {}
  setItemEnabled(_id: string, _isEnabled: boolean) {}
  setItemAllowedIncognito(_id: string, _isAllowedIncognito: boolean) {}
  setItemAllowedOnFileUrls(_id: string, _isAllowedOnFileUrls: boolean) {}
  setItemHostAccess(
      _id: string, _hostAccess: chrome.developerPrivate.HostAccess) {}
  setItemCollectsErrors(_id: string, _collectsErrors: boolean) {}
  setItemPinnedToToolbar(_id: string, _pinnedToToolbar: boolean) {}
  inspectItemView(_id: string, _view: chrome.developerPrivate.ExtensionView) {}
  openUrl(_url: string) {}


  reloadItem(_id: string) {
    return Promise.resolve();
  }

  repairItem(_id: string) {}
  showItemOptionsPage(_extension: chrome.developerPrivate.ExtensionInfo) {}
  showInFolder(_id: string) {}

  getExtensionSize(_id: string) {
    return Promise.resolve('10 MB');
  }

  addRuntimeHostPermission(_id: string, _host: string) {
    return Promise.resolve();
  }

  removeRuntimeHostPermission(_id: string, _host: string) {
    return Promise.resolve();
  }

  setShowAccessRequestsInToolbar(_id: string, _showRequests: boolean) {}

  recordUserAction(_metricName: string) {}

  getItemStateChangedTarget() {
    return this.itemStateChangedTarget;
  }
}

/**
 * A mock to intercept User Action logging calls and verify how many times they
 * were called.
 */
export class MetricsPrivateMock {
  userActionMap: Map<string, number> = new Map();

  getUserActionCount(metricName: string): number {
    return this.userActionMap.get(metricName) || 0;
  }

  recordUserAction(metricName: string) {
    this.userActionMap.set(metricName, this.getUserActionCount(metricName) + 1);
  }
}

export function isElementVisible(element: HTMLElement): boolean {
  const rect = element.getBoundingClientRect();
  return rect.width * rect.height > 0;  // Width and height is never negative.
}

/**
 * Tests that the element's visibility matches |expectedVisible| and,
 * optionally, has specific content if it is visible.
 * @param parentEl The parent element to query for the element.
 * @param selector The selector to find the element.
 * @param expectedVisible Whether the element should be visible.
 * @param expectedText The expected textContent value.
 */
export function testVisible(
    parentEl: HTMLElement, selector: string, expectedVisible: boolean,
    expectedText?: string) {
  const visible = isChildVisible(parentEl, selector);
  assertEquals(expectedVisible, visible, selector);
  if (expectedVisible && visible && expectedText) {
    const element = parentEl.shadowRoot!.querySelector(selector)!;
    assertEquals(expectedText, element.textContent!.trim(), selector);
  }
}

/**
 * Creates an ExtensionInfo object.
 * @param properties A set of properties that will be used on the resulting
 *     ExtensionInfo (otherwise defaults will be used).
 */
export function createExtensionInfo(
    properties?: Partial<chrome.developerPrivate.ExtensionInfo>):
    chrome.developerPrivate.ExtensionInfo {
  const id = properties && properties.hasOwnProperty('id') ? properties['id']! :
                                                             'a'.repeat(32);
  const baseUrl = 'chrome-extension://' + id + '/';
  const dummy = createDummyExtensionInfo();

  // Modify some dummy properties for testing.
  dummy.description = 'This is an extension';
  dummy.iconUrl = 'chrome://extension-icon/' + id + '/24/0';
  dummy.id = id;
  dummy.incognitoAccess = {isEnabled: true, isActive: false};
  dummy.name = 'Wonderful Extension';
  dummy.location = chrome.developerPrivate.Location.FROM_STORE;
  dummy.userMayModify = true;

  function createDummyView(url: string): chrome.developerPrivate.ExtensionView {
    return {
      url: url,
      renderProcessId: 0,
      renderViewId: 0,
      incognito: false,
      isIframe: false,
      type: chrome.developerPrivate.ViewType.APP_WINDOW,
    };
  }
  dummy.views = [
    createDummyView(baseUrl + 'foo.html'),
    createDummyView(baseUrl + 'bar.html'),
  ];
  return Object.assign(dummy, properties || {});
}

/**
 * Finds all nodes matching |query| under |root|, within self and children's
 * Shadow DOM.
 */
export function findMatches(
    root: HTMLElement|Document, query: string): HTMLElement[] {
  const elements = new Set<HTMLElement>();
  function doSearch(node: Node) {
    if (node.nodeType === Node.ELEMENT_NODE) {
      const matches = (node as Element).querySelectorAll<HTMLElement>(query);
      for (const match of matches) {
        elements.add(match);
      }
    }
    let child = node.firstChild;
    while (child !== null) {
      doSearch(child);
      child = child.nextSibling;
    }
    const shadowRoot = (node as HTMLElement).shadowRoot;
    if (shadowRoot) {
      doSearch(shadowRoot);
    }
  }
  doSearch(root);
  return Array.from(elements);
}
