// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-item. */

import {ExtensionsItemElement, IronIconElement, navigation, Page} from 'chrome://extensions/extensions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';

import {TestService} from './test_service.js';
import {createExtensionInfo, MockItemDelegate, testVisible} from './test_util.js';

/**
 * The data used to populate the extension item.
 */
const extensionData: chrome.developerPrivate.ExtensionInfo =
    createExtensionInfo();

// The normal elements, which should always be shown.
const normalElements = [
  {selector: '#name', text: extensionData.name},
  {selector: '#icon'},
  {selector: '#description', text: extensionData.description},
  {selector: '#enableToggle'},
  {selector: '#detailsButton'},
  {selector: '#removeButton'},
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
    text: '1 more…',
  },
];

/**
 * Tests that the elements' visibility matches the expected visibility.
 */
function testElementsVisibility(
    item: HTMLElement, elements: Array<{selector: string, text?: string}>,
    visibility: boolean): void {
  elements.forEach(function(element) {
    testVisible(item, element.selector, visibility, element.text);
  });
}

/** Tests that normal elements are visible. */
function testNormalElementsAreVisible(item: HTMLElement): void {
  testElementsVisibility(item, normalElements, true);
}

/** Tests that dev elements are visible. */
function testDeveloperElementsAreVisible(item: HTMLElement): void {
  testElementsVisibility(item, devElements, true);
}

/** Tests that dev elements are hidden. */
function testDeveloperElementsAreHidden(item: HTMLElement): void {
  testElementsVisibility(item, devElements, false);
}

suite('ExtensionItemTest', function() {
  /**
   * Extension item created before each test.
   */
  let item: ExtensionsItemElement;

  let mockDelegate: MockItemDelegate;

  // Initialize an extension item before each test.
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    mockDelegate = new MockItemDelegate();
    item = document.createElement('extensions-item');
    item.data = createExtensionInfo();
    item.delegate = mockDelegate;
    document.body.appendChild(item);
    const toastManager = document.createElement('cr-toast-manager');
    document.body.appendChild(toastManager);
  });

  test('ElementVisibilityNormalState', function() {
    testNormalElementsAreVisible(item);
    testDeveloperElementsAreHidden(item);

    assertTrue(item.$.enableToggle.checked);
    item.set('data.state', 'DISABLED');
    assertFalse(item.$.enableToggle.checked);
    item.set('data.state', 'BLACKLISTED');
    assertFalse(item.$.enableToggle.checked);
  });

  test('ElementVisibilityDeveloperState', function() {
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

    item.set('data.disableReasons.reloading', true);
    flush();
    testVisible(item, '#dev-reload-button', true);

    item.set('data.disableReasons.reloading', false);
    flush();
    item.set('data.state', chrome.developerPrivate.ExtensionState.TERMINATED);
    flush();
    testVisible(item, '#dev-reload-button', false);
    testVisible(item, '#enableToggle', false);
  });

  /** Tests that the delegate methods are correctly called. */
  test('ClickableItems', function() {
    item.set('inDevMode', true);

    mockDelegate.testClickingCalls(
        item.$.removeButton, 'deleteItem', [item.data.id]);
    mockDelegate.testClickingCalls(
        item.$.enableToggle, 'setItemEnabled', [item.data.id, false]);
    mockDelegate.testClickingCalls(
        item.shadowRoot!.querySelector<HTMLElement>(
            '#inspect-views a[is="action-link"]')!,
        'inspectItemView', [item.data.id, item.data.views[0]]);

    // Setup for testing navigation buttons.
    let currentPage = null;
    navigation.addListener(newPage => {
      currentPage = newPage;
    });

    item.$.detailsButton.click();
    assertDeepEquals(
        currentPage, {page: Page.DETAILS, extensionId: item.data.id});

    // Reset current page and test inspect-view navigation.
    navigation.navigateTo({page: Page.LIST});
    currentPage = null;
    item.shadowRoot!
        .querySelector<HTMLElement>(
            '#inspect-views a[is="action-link"]:nth-of-type(2)')!.click();
    assertDeepEquals(
        currentPage, {page: Page.DETAILS, extensionId: item.data.id});

    item.set('data.disableReasons.corruptInstall', true);
    flush();
    mockDelegate.testClickingCalls(
        item.shadowRoot!.querySelector<HTMLElement>('#repair-button')!,
        'repairItem', [item.data.id]);
    testVisible(item, '#enableToggle', false);
    item.set('data.disableReasons.corruptInstall', false);
    flush();

    item.set('data.state', chrome.developerPrivate.ExtensionState.TERMINATED);
    flush();
    mockDelegate.testClickingCalls(
        item.shadowRoot!.querySelector<HTMLElement>(
            '#terminated-reload-button')!,
        'reloadItem', [item.data.id], Promise.resolve());

    item.set('data.location', chrome.developerPrivate.Location.UNPACKED);
    item.set('data.state', chrome.developerPrivate.ExtensionState.ENABLED);
    flush();
  });

  /** Tests that the reload button properly fires the load-error event. */
  test(
      'FailedReloadFiresLoadError', async function() {
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

        function verifyEventPromise(expectCalled: boolean): Promise<void> {
          return new Promise((resolve, _reject) => {
            setTimeout(() => {
              assertEquals(expectCalled, firedLoadError);
              resolve();
            });
          });
        }

        item.shadowRoot!.querySelector<HTMLElement>(
                            '#dev-reload-button')!.click();
        let id = await proxyDelegate.whenCalled('reloadItem');
        assertEquals(item.data.id, id);
        await verifyEventPromise(false);
        proxyDelegate.resetResolver('reloadItem');
        proxyDelegate.setForceReloadItemError(true);
        item.shadowRoot!.querySelector<HTMLElement>(
                            '#dev-reload-button')!.click();
        id = await proxyDelegate.whenCalled('reloadItem');
        assertEquals(item.data.id, id);
        return verifyEventPromise(true);
      });

  test('Warnings', function() {
    const kCorrupt = 1 << 0;
    const kSuspicious = 1 << 1;
    const kBlacklisted = 1 << 2;
    const kRuntime = 1 << 3;
    const kSafeBrowsingAllowlist = 1 << 4;

    function assertWarnings(mask: number) {
      assertEquals(
          !!(mask & kCorrupt), isChildVisible(item, '#corrupted-warning'));
      assertEquals(
          !!(mask & kSuspicious), isChildVisible(item, '#suspicious-warning'));
      assertEquals(
          !!(mask & kBlacklisted),
          isChildVisible(item, '#blacklisted-warning'));
      assertEquals(
          !!(mask & kRuntime), isChildVisible(item, '#runtime-warnings'));
      assertEquals(
          !!(mask & kSafeBrowsingAllowlist),
          isChildVisible(item, '#allowlist-warning'));
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

    item.set('data.showSafeBrowsingAllowlistWarning', true);
    flush();
    assertWarnings(kSafeBrowsingAllowlist);

    // Test that the allowlist warning is not shown when there is already a
    // warning message.
    item.set('data.disableReasons.suspiciousInstall', true);
    flush();
    assertWarnings(kSuspicious);
  });

  test('SourceIndicator', function() {
    assertFalse(isChildVisible(item, '#source-indicator'));
    item.set('data.location', 'UNPACKED');
    flush();
    assertTrue(isChildVisible(item, '#source-indicator'));
    const icon = item.shadowRoot!.querySelector<IronIconElement>(
        '#source-indicator iron-icon');
    assertTrue(!!icon);
    assertEquals('extensions-icons:unpacked', icon.icon);

    item.set('data.location', 'THIRD_PARTY');
    flush();
    assertTrue(isChildVisible(item, '#source-indicator'));
    assertEquals('extensions-icons:input', icon.icon);

    item.set('data.location', 'UNKNOWN');
    flush();
    assertTrue(isChildVisible(item, '#source-indicator'));
    assertEquals('extensions-icons:input', icon.icon);

    item.set('data.location', 'INSTALLED_BY_DEFAULT');
    flush();
    assertFalse(isChildVisible(item, '#source-indicator'));

    item.set('data.location', 'FROM_STORE');
    item.set('data.controlledInfo', {text: 'policy'});
    flush();
    assertTrue(isChildVisible(item, '#source-indicator'));
    assertEquals('extensions-icons:business', icon.icon);

    item.set('data.controlledInfo', null);
    flush();
    assertFalse(isChildVisible(item, '#source-indicator'));
  });

  test('EnableToggle', function() {
    assertFalse(item.$.enableToggle.disabled);

    // Test case where user does not have permission.
    item.set('data.userMayModify', false);
    flush();
    assertTrue(item.$.enableToggle.disabled);
    // Reset state.
    item.set('data.userMayModify', true);
    flush();

    // Test case of a blacklisted extension.
    item.set('data.state', 'BLACKLISTED');
    flush();
    assertTrue(item.$.enableToggle.disabled);
    // Reset state.
    item.set('data.state', 'ENABLED');
    flush();

    // This section tests that the enable toggle is visible but disabled
    // when disableReasons.blockedByPolicy is true. This test prevents a
    // regression to crbug/1003014.
    item.set('data.disableReasons.blockedByPolicy', true);
    flush();
    testVisible(item, '#enableToggle', true);
    assertTrue(item.$.enableToggle.disabled);
    item.set('data.disableReasons.blockedByPolicy', false);
    flush();

    item.set('data.disableReasons.publishedInStoreRequired', true);
    flush();
    testVisible(item, '#enableToggle', true);
    assertTrue(item.$.enableToggle.disabled);
    item.set('data.disableReasons.publishedInStoreRequired', false);
    flush();

    testVisible(item, '#parentDisabledPermissionsToolTip', false);
    item.set('data.disableReasons.parentDisabledPermissions', true);
    flush();
    testVisible(item, '#enableToggle', true);
    assertFalse(item.$.enableToggle.disabled);
    testVisible(item, '#parentDisabledPermissionsToolTip', true);
    item.set('data.disableReasons.parentDisabledPermissions', false);
    flush();

    item.set('data.disableReasons.custodianApprovalRequired', true);
    flush();
    testVisible(item, '#enableToggle', true);
    assertFalse(item.$.enableToggle.disabled);
    item.set('data.disableReasons.custodianApprovalRequired', false);
    flush();
  });

  test('RemoveButton', function() {
    assertFalse(item.$.removeButton.hidden);
    item.set('data.mustRemainInstalled', true);
    flush();
    assertTrue(item.$.removeButton.hidden);
  });

  test('HtmlInName', function() {
    const name = '<HTML> in the name!';
    item.set('data.name', name);
    flush();
    assertEquals(name, item.$.name.textContent!.trim());
    // "Related to $1" is IDS_MD_EXTENSIONS_EXTENSION_A11Y_ASSOCIATION.
    assertEquals(
        `Related to ${name}`, item.$.a11yAssociation.textContent!.trim());
  });

  test('RepairButton', function() {
    // For most extensions, the "repair" button should be displayed if the
    // extension is detected as corrupted.
    testVisible(item, '#repair-button', false);
    item.set('data.disableReasons.corruptInstall', true);
    flush();
    testVisible(item, '#repair-button', true);
    item.set('data.disableReasons.corruptInstall', false);
    flush();
    testVisible(item, '#repair-button', false);

    // However, the user isn't allowed to initiate a repair for extensions they
    // aren't allowed to modify, so the button shouldn't be visible.
    item.set('data.userMayModify', false);
    item.set('data.disableReasons.corruptInstall', true);
    flush();
    testVisible(item, '#repair-button', false);
  });

  test(
      'InspectableViewSortOrder', function() {
        function getUrl(path: string) {
          return `chrome-extension://${extensionData.id}/${path}`;
        }
        item.set('data.views', [
          {
            type: chrome.developerPrivate.ViewType.EXTENSION_POPUP,
            url: getUrl('popup.html'),
          },
          {
            type: chrome.developerPrivate.ViewType.EXTENSION_BACKGROUND_PAGE,
            url: getUrl('_generated_background_page.html'),
          },
          {
            type: chrome.developerPrivate.ViewType
                      .EXTENSION_SERVICE_WORKER_BACKGROUND,
            url: getUrl('sw.js'),
          },
        ]);
        item.set('inDevMode', true);
        flush();

        // Check that when multiple views are available, the service worker is
        // sorted first.
        assertEquals(
            'service worker,',
            item.shadowRoot!
                .querySelector<HTMLElement>(
                    '#inspect-views a:first-of-type')!.textContent!.trim());
      });
});
