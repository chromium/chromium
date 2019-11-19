// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-item. */

import {navigation, Page} from 'chrome://extensions/extensions.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {tap} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isVisible} from '../test_util.m.js';

import {TestService} from './test_service.js';
import {createExtensionInfo, MockItemDelegate, testVisible} from './test_util.js';

/**
 * The data used to populate the extension item.
 * @type {chrome.developerPrivate.ExtensionInfo}
 */
const extensionData = createExtensionInfo();

// The normal elements, which should always be shown.
const normalElements = [
  {selector: '#name', text: extensionData.name},
  {selector: '#icon'},
  {selector: '#description', text: extensionData.description},
  {selector: '#enable-toggle'},
  {selector: '#detailsButton'},
  {selector: '#remove-button'},
];
// The developer elements, which should only be shown if in developer
// mode *and* showing details.
const devElements = [
  {selector: '#version', text: extensionData.version},
  {selector: '#extension-id', text: `ID: ${extensionData.id}`},
  {selector: '#inspect-views'},
  {selector: '#inspect-views a[is="action-link"]', text: 'foo.html,'},
  {
    selector: '#inspect-views a[is="action-link"]:nth-of-type(2)',
    text: '1 more…'
  },
];

/**
 * Tests that the elements' visibility matches the expected visibility.
 * @param {Item} item
 * @param {Array<Object<string>>} elements
 * @param {boolean} visibility
 */
function testElementsVisibility(item, elements, visibility) {
  elements.forEach(function(element) {
    testVisible(item, element.selector, visibility, element.text);
  });
}

/** Tests that normal elements are visible. */
function testNormalElementsAreVisible(item) {
  testElementsVisibility(item, normalElements, true);
}

/** Tests that normal elements are hidden. */
function testNormalElementsAreHidden(item) {
  testElementsVisibility(item, normalElements, false);
}

/** Tests that dev elements are visible. */
function testDeveloperElementsAreVisible(item) {
  testElementsVisibility(item, devElements, true);
}

/** Tests that dev elements are hidden. */
function testDeveloperElementsAreHidden(item) {
  testElementsVisibility(item, devElements, false);
}

window.extension_item_tests = {};
extension_item_tests.suiteName = 'ExtensionItemTest';
/** @enum {string} */
extension_item_tests.TestNames = {
  ElementVisibilityNormalState: 'element visibility: normal state',
  ElementVisibilityDeveloperState:
      'element visibility: after enabling developer mode',
  ClickableItems: 'clickable items',
  FailedReloadFiresLoadError: 'failed reload fires load error',
  Warnings: 'warnings',
  SourceIndicator: 'source indicator',
  EnableToggle: 'toggle is disabled when necessary',
  RemoveButton: 'remove button hidden when necessary',
  HtmlInName: 'html in extension name',
};

suite(extension_item_tests.suiteName, function() {
  /**
   * Extension item created before each test.
   * @type {Item}
   */
  let item;

  /** @type {MockItemDelegate} */
  let mockDelegate;

  // Initialize an extension item before each test.
  setup(function() {
    PolymerTest.clearBody();
    mockDelegate = new MockItemDelegate();
    item = document.createElement('extensions-item');
    item.data = createExtensionInfo();
    item.delegate = mockDelegate;
    document.body.appendChild(item);
    const toastManager = document.createElement('cr-toast-manager');
    document.body.appendChild(toastManager);
  });

  test(
      assert(extension_item_tests.TestNames.ElementVisibilityNormalState),
      function() {
        testNormalElementsAreVisible(item);
        testDeveloperElementsAreHidden(item);

        expectTrue(item.$['enable-toggle'].checked);
        item.set('data.state', 'DISABLED');
        expectFalse(item.$['enable-toggle'].checked);
        item.set('data.state', 'BLACKLISTED');
        expectFalse(item.$['enable-toggle'].checked);
      });

  test(
      assert(extension_item_tests.TestNames.ElementVisibilityDeveloperState),
      function() {
        item.set('inDevMode', true);

        testNormalElementsAreVisible(item);
        testDeveloperElementsAreVisible(item);

        // Developer reload button should be visible only for enabled unpacked
        // extensions.
        testVisible(item, '#dev-reload-button', false);

        item.set('data.location', chrome.developerPrivate.Location.UNPACKED);
        flush();
        testVisible(item, '#dev-reload-button', true);

        item.set('data.state', chrome.developerPrivate.ExtensionState.DISABLED);
        flush();
        testVisible(item, '#dev-reload-button', false);

        item.set(
            'data.state', chrome.developerPrivate.ExtensionState.TERMINATED);
        flush();
        testVisible(item, '#dev-reload-button', false);
      });

  /** Tests that the delegate methods are correctly called. */
  test(assert(extension_item_tests.TestNames.ClickableItems), function() {
    item.set('inDevMode', true);

    mockDelegate.testClickingCalls(
        item.$['remove-button'], 'deleteItem', [item.data.id]);
    mockDelegate.testClickingCalls(
        item.$['enable-toggle'], 'setItemEnabled', [item.data.id, false]);
    mockDelegate.testClickingCalls(
        item.$$('#inspect-views a[is="action-link"]'), 'inspectItemView',
        [item.data.id, item.data.views[0]]);

    // Setup for testing navigation buttons.
    let currentPage = null;
    navigation.addListener(newPage => {
      currentPage = newPage;
    });

    tap(item.$$('#detailsButton'));
    expectDeepEquals(
        currentPage, {page: Page.DETAILS, extensionId: item.data.id});

    // Reset current page and test inspect-view navigation.
    navigation.navigateTo({page: Page.LIST});
    currentPage = null;
    tap(item.$$('#inspect-views a[is="action-link"]:nth-of-type(2)'));
    expectDeepEquals(
        currentPage, {page: Page.DETAILS, extensionId: item.data.id});

    item.set('data.disableReasons.corruptInstall', true);
    flush();
    mockDelegate.testClickingCalls(
        item.$$('#repair-button'), 'repairItem', [item.data.id]);

    item.set('data.state', chrome.developerPrivate.ExtensionState.TERMINATED);
    flush();
    mockDelegate.testClickingCalls(
        item.$$('#terminated-reload-button'), 'reloadItem', [item.data.id],
        Promise.resolve());

    item.set('data.location', chrome.developerPrivate.Location.UNPACKED);
    item.set('data.state', chrome.developerPrivate.ExtensionState.ENABLED);
    flush();
  });

  /** Tests that the reload button properly fires the load-error event. */
  test(
      assert(extension_item_tests.TestNames.FailedReloadFiresLoadError),
      function() {
        item.set('inDevMode', true);
        item.set('data.location', chrome.developerPrivate.Location.UNPACKED);
        flush();
        testVisible(item, '#dev-reload-button', true);

        // Check clicking the reload button. The reload button should fire a
        // load-error event if and only if the reload fails (indicated by a
        // rejected promise).
        // This is a bit of a pain to verify because the promises finish
        // asynchronously, so we have to use setTimeout()s.
        let firedLoadError = false;
        item.addEventListener('load-error', () => {
          firedLoadError = true;
        });

        // This is easier to test with a TestBrowserProxy-style delegate.
        const proxyDelegate = new TestService();
        item.delegate = proxyDelegate;

        const verifyEventPromise = function(expectCalled) {
          return new Promise((resolve, reject) => {
            setTimeout(() => {
              expectEquals(expectCalled, firedLoadError);
              resolve();
            });
          });
        };

        tap(item.$$('#dev-reload-button'));
        return proxyDelegate.whenCalled('reloadItem')
            .then(function(id) {
              expectEquals(item.data.id, id);
              return verifyEventPromise(false);
            })
            .then(function() {
              proxyDelegate.resetResolver('reloadItem');
              proxyDelegate.setForceReloadItemError(true);
              tap(item.$$('#dev-reload-button'));
              return proxyDelegate.whenCalled('reloadItem');
            })
            .then(function(id) {
              expectEquals(item.data.id, id);
              return verifyEventPromise(true);
            });
      });

  test(assert(extension_item_tests.TestNames.Warnings), function() {
    const kCorrupt = 1 << 0;
    const kSuspicious = 1 << 1;
    const kBlacklisted = 1 << 2;
    const kRuntime = 1 << 3;

    function assertWarnings(mask) {
      assertEquals(!!(mask & kCorrupt), isVisible(item, '#corrupted-warning'));
      assertEquals(
          !!(mask & kSuspicious), isVisible(item, '#suspicious-warning'));
      assertEquals(
          !!(mask & kBlacklisted), isVisible(item, '#blacklisted-warning'));
      assertEquals(!!(mask & kRuntime), isVisible(item, '#runtime-warnings'));
    }

    assertWarnings(0);

    item.set('data.disableReasons.corruptInstall', true);
    flush();
    assertWarnings(kCorrupt);

    item.set('data.disableReasons.suspiciousInstall', true);
    flush();
    assertWarnings(kCorrupt | kSuspicious);

    item.set('data.blacklistText', 'This item is blacklisted');
    flush();
    assertWarnings(kCorrupt | kSuspicious | kBlacklisted);

    item.set('data.blacklistText', null);
    flush();
    assertWarnings(kCorrupt | kSuspicious);

    item.set('data.runtimeWarnings', ['Dummy warning']);
    flush();
    assertWarnings(kCorrupt | kSuspicious | kRuntime);

    item.set('data.disableReasons.corruptInstall', false);
    item.set('data.disableReasons.suspiciousInstall', false);
    item.set('data.runtimeWarnings', []);
    flush();
    assertWarnings(0);
  });

  test(assert(extension_item_tests.TestNames.SourceIndicator), function() {
    expectFalse(isVisible(item, '#source-indicator'));
    item.set('data.location', 'UNPACKED');
    flush();
    expectTrue(isVisible(item, '#source-indicator'));
    const icon = item.$$('#source-indicator iron-icon');
    assertTrue(!!icon);
    expectEquals('extensions-icons:unpacked', icon.icon);

    item.set('data.location', 'THIRD_PARTY');
    flush();
    expectTrue(isVisible(item, '#source-indicator'));
    expectEquals('extensions-icons:input', icon.icon);

    item.set('data.location', 'UNKNOWN');
    flush();
    expectTrue(isVisible(item, '#source-indicator'));
    expectEquals('extensions-icons:input', icon.icon);

    item.set('data.location', 'FROM_STORE');
    item.set('data.controlledInfo', {type: 'POLICY', text: 'policy'});
    flush();
    expectTrue(isVisible(item, '#source-indicator'));
    expectEquals('extensions-icons:business', icon.icon);

    item.set('data.controlledInfo', null);
    flush();
    expectFalse(isVisible(item, '#source-indicator'));
  });

  test(assert(extension_item_tests.TestNames.EnableToggle), function() {
    expectFalse(item.$['enable-toggle'].disabled);

    // Test case where user does not have permission.
    item.set('data.userMayModify', false);
    flush();
    expectTrue(item.$['enable-toggle'].disabled);

    // Test case of a blacklisted extension.
    item.set('data.userMayModify', true);
    item.set('data.state', 'BLACKLISTED');
    flush();
    expectTrue(item.$['enable-toggle'].disabled);
  });

  test(assert(extension_item_tests.TestNames.RemoveButton), function() {
    expectFalse(item.$['remove-button'].hidden);
    item.set('data.controlledInfo', {type: 'POLICY', text: 'policy'});
    flush();
    expectTrue(item.$['remove-button'].hidden);
  });

  test(assert(extension_item_tests.TestNames.HtmlInName), function() {
    let name = '<HTML> in the name!';
    item.set('data.name', name);
    flush();
    assertEquals(name, item.$.name.textContent.trim());
    // "Related to $1" is IDS_MD_EXTENSIONS_EXTENSION_A11Y_ASSOCIATION.
    assertEquals(
        `Related to ${name}`, item.$.a11yAssociation.textContent.trim());
  });
});
