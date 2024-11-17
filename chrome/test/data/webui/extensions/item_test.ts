// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-item. */

import type {CrIconElement, ExtensionsItemElement} from 'chrome://extensions/extensions.js';
import {Mv2ExperimentStage, navigation, Page} from 'chrome://extensions/extensions.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

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

  test('ElementVisibilityNormalState', async () => {
    testNormalElementsAreVisible(item);
    testDeveloperElementsAreHidden(item);

    assertTrue(item.$.enableToggle.checked);

    let data = createExtensionInfo(item.data);
    data.state = chrome.developerPrivate.ExtensionState.DISABLED;
    item.data = data;
    await microtasksFinished();
    assertFalse(item.$.enableToggle.checked);

    data = createExtensionInfo(item.data);
    data.state = chrome.developerPrivate.ExtensionState.BLOCKLISTED;
    item.data = data;
    await microtasksFinished();
    assertFalse(item.$.enableToggle.checked);
  });

  test('ElementVisibilityDeveloperState', async () => {
    item.inDevMode = true;
    await microtasksFinished();

    testNormalElementsAreVisible(item);
    testDeveloperElementsAreVisible(item);

    // Developer reload button should be visible only for enabled unpacked
    // extensions.
    testVisible(item, '#dev-reload-button', false);

    let data = createExtensionInfo(item.data);
    data.location = chrome.developerPrivate.Location.UNPACKED;
    item.data = data;
    await microtasksFinished();
    testVisible(item, '#dev-reload-button', true);

    data = createExtensionInfo(item.data);
    data.state = chrome.developerPrivate.ExtensionState.DISABLED;
    item.data = data;
    await microtasksFinished();
    testVisible(item, '#dev-reload-button', false);

    data = createExtensionInfo(item.data);
    data.disableReasons.reloading = true;
    item.data = data;
    await microtasksFinished();
    testVisible(item, '#dev-reload-button', true);

    data = createExtensionInfo(item.data);
    data.disableReasons.reloading = false;
    data.state = chrome.developerPrivate.ExtensionState.TERMINATED;
    item.data = data;
    await microtasksFinished();
    testVisible(item, '#dev-reload-button', false);
    testVisible(item, '#enableToggle', false);
  });

  /** Tests that the delegate methods are correctly called. */
  test('ClickableItems', async function() {
    item.inDevMode = true;
    await microtasksFinished();

    await mockDelegate.testClickingCalls(
        item.$.removeButton, 'deleteItem', [item.data.id]);
    await mockDelegate.testClickingCalls(
        item.$.enableToggle, 'setItemEnabled', [item.data.id, false]);
    await mockDelegate.testClickingCalls(
        item.shadowRoot!.querySelector<HTMLElement>(
            '#inspect-views a[is="action-link"]')!,
        'inspectItemView', [item.data.id, item.data.views[0]]);

    // Setup for testing navigation buttons.
    let currentPage = null;
    navigation.addListener(newPage => {
      currentPage = newPage;
    });

    item.$.detailsButton.click();
    await microtasksFinished();
    assertDeepEquals(
        currentPage, {page: Page.DETAILS, extensionId: item.data.id});

    // Reset current page and test inspect-view navigation.
    navigation.navigateTo({page: Page.LIST});
    currentPage = null;
    item.shadowRoot!
        .querySelector<HTMLElement>(
            '#inspect-views a[is="action-link"]:nth-of-type(2)')!.click();
    await microtasksFinished();
    assertDeepEquals(
        currentPage, {page: Page.DETAILS, extensionId: item.data.id});

    let data = createExtensionInfo(item.data);
    data.disableReasons.corruptInstall = true;
    item.data = data;
    await microtasksFinished();
    await mockDelegate.testClickingCalls(
        item.shadowRoot!.querySelector<HTMLElement>('#repair-button')!,
        'repairItem', [item.data.id]);
    testVisible(item, '#enableToggle', false);

    data = createExtensionInfo(item.data);
    data.disableReasons.corruptInstall = false;
    data.state = chrome.developerPrivate.ExtensionState.TERMINATED;
    item.data = data;
    await microtasksFinished();

    await mockDelegate.testClickingCalls(
        item.shadowRoot!.querySelector<HTMLElement>(
            '#terminated-reload-button')!,
        'reloadItem', [item.data.id], Promise.resolve());

    data = createExtensionInfo(item.data);
    data.location = chrome.developerPrivate.Location.UNPACKED;
    data.state = chrome.developerPrivate.ExtensionState.ENABLED;
    item.data = data;
    await microtasksFinished();
  });

  /** Tests that the reload button properly fires the load-error event. */
  test('FailedReloadFiresLoadError', async function() {
    item.inDevMode = true;
    const data = createExtensionInfo(item.data);
    data.location = chrome.developerPrivate.Location.UNPACKED;
    item.data = data;
    await microtasksFinished();

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

    item.shadowRoot!.querySelector<HTMLElement>('#dev-reload-button')!.click();
    let id = await proxyDelegate.whenCalled('reloadItem');
    assertEquals(item.data.id, id);
    await verifyEventPromise(false);
    proxyDelegate.resetResolver('reloadItem');
    proxyDelegate.setForceReloadItemError(true);
    item.shadowRoot!.querySelector<HTMLElement>('#dev-reload-button')!.click();
    id = await proxyDelegate.whenCalled('reloadItem');
    assertEquals(item.data.id, id);
    return verifyEventPromise(true);
  });

  test('Description', async () => {
    // Description is visible if there are no warnings.
    assertTrue(isChildVisible(item, '#description'));

    // Description is hidden if there is a severe warning.
    let data = createExtensionInfo(item.data);
    data.disableReasons.corruptInstall = true;
    item.data = data;
    await microtasksFinished();
    assertFalse(isChildVisible(item, '#description'));

    // Description is hidden if there is a MV2 deprecation warning.
    data = createExtensionInfo(item.data);
    data.disableReasons.corruptInstall = false;
    data.disableReasons.unsupportedManifestVersion = true;
    item.data = data;
    await microtasksFinished();
    assertFalse(isChildVisible(item, '#description'));

    // Description is hidden if there is an allowlist warning.
    data = createExtensionInfo(item.data);
    data.disableReasons.unsupportedManifestVersion = false;
    data.showSafeBrowsingAllowlistWarning = true;
    item.data = data;
    await microtasksFinished();
    assertFalse(isChildVisible(item, '#description'));
  });

  test('Warnings', async () => {
    // Severe warnings.
    const kCorrupt = 1 << 0;
    const kSuspicious = 1 << 1;
    const kBlocklisted = 1 << 2;
    const kRuntime = 1 << 3;
    // Allowlist warning.
    const kSafeBrowsingAllowlist = 1 << 4;
    // MV2 deprecation warning.
    const kMv2Deprecation = 1 << 5;

    function assertWarnings(mask: number) {
      assertEquals(
          !!(mask & kCorrupt), isChildVisible(item, '#corrupted-warning'));
      assertEquals(
          !!(mask & kSuspicious), isChildVisible(item, '#suspicious-warning'));
      assertEquals(
          !!(mask & kBlocklisted),
          isChildVisible(item, '#blocklisted-warning'));
      assertEquals(
          !!(mask & kRuntime), isChildVisible(item, '#runtime-warnings'));
      assertEquals(
          !!(mask & kSafeBrowsingAllowlist),
          isChildVisible(item, '#allowlist-warning'));
      assertEquals(
          !!(mask & kMv2Deprecation),
          isChildVisible(item, '#mv2-deprecation-warning'));
    }

    assertWarnings(0);

    // Show severe warnings by updating the corresponding properties.
    let data = createExtensionInfo(item.data);
    data.disableReasons.corruptInstall = true;
    item.data = data;
    await microtasksFinished();
    assertWarnings(kCorrupt);

    data = createExtensionInfo(item.data);
    data.disableReasons.suspiciousInstall = true;
    item.data = data;
    await microtasksFinished();
    assertWarnings(kCorrupt | kSuspicious);

    data = createExtensionInfo(item.data);
    data.blocklistText = 'This item is blocklisted';
    item.data = data;
    await microtasksFinished();
    assertWarnings(kCorrupt | kSuspicious | kBlocklisted);

    data = createExtensionInfo(item.data);
    data.blocklistText = undefined;
    item.data = data;
    await microtasksFinished();
    assertWarnings(kCorrupt | kSuspicious);

    data = createExtensionInfo(item.data);
    data.runtimeWarnings = ['Dummy warning'];
    item.data = data;
    await microtasksFinished();
    assertWarnings(kCorrupt | kSuspicious | kRuntime);

    // Reset all properties affecting warnings.
    data = createExtensionInfo(item.data);
    data.disableReasons.corruptInstall = false;
    data.disableReasons.suspiciousInstall = false;
    data.runtimeWarnings = [];
    item.data = data;
    await microtasksFinished();
    assertWarnings(0);

    // Show MV2 deprecation warning.
    data = createExtensionInfo(item.data);
    data.disableReasons.unsupportedManifestVersion = true;
    item.data = data;
    await microtasksFinished();
    assertWarnings(kMv2Deprecation);

    // MV2 deprecation warning is hidden if there are any severe warnings.
    data = createExtensionInfo(item.data);
    data.disableReasons.suspiciousInstall = true;
    item.data = data;
    await microtasksFinished();
    assertWarnings(kSuspicious);

    // Reset all properties affecting warnings.
    data = createExtensionInfo(item.data);
    data.disableReasons.unsupportedManifestVersion = false;
    data.disableReasons.suspiciousInstall = false;
    item.data = data;
    await microtasksFinished();
    assertWarnings(0);

    // Show allowlist warning.
    data = createExtensionInfo(item.data);
    data.showSafeBrowsingAllowlistWarning = true;
    item.data = data;
    await microtasksFinished();
    assertWarnings(kSafeBrowsingAllowlist);
    // Allowlist warning is not visible if there are any severe warnings.
    data = createExtensionInfo(item.data);
    data.disableReasons.suspiciousInstall = true;
    item.data = data;
    await microtasksFinished();
    assertWarnings(kSuspicious);
    // Allowlist warning is not visible if there is a MV2 deprecation warning
    data = createExtensionInfo(item.data);
    data.disableReasons.suspiciousInstall = false;
    data.disableReasons.unsupportedManifestVersion = true;
    item.data = data;
    await microtasksFinished();
    assertWarnings(kMv2Deprecation);
  });

  test('SourceIndicator', async () => {
    assertFalse(isChildVisible(item, '#source-indicator'));
    let data = createExtensionInfo(item.data);
    data.location = chrome.developerPrivate.Location.UNPACKED;
    item.data = data;
    await microtasksFinished();
    assertTrue(isChildVisible(item, '#source-indicator'));
    let icon = item.shadowRoot!.querySelector<CrIconElement>(
        '#source-indicator cr-icon');
    assertTrue(!!icon);
    assertEquals('extensions-icons:unpacked', icon.icon);

    data = createExtensionInfo(item.data);
    data.location = chrome.developerPrivate.Location.THIRD_PARTY;
    item.data = data;
    await microtasksFinished();
    assertTrue(isChildVisible(item, '#source-indicator'));
    assertEquals('extensions-icons:input', icon.icon);

    data = createExtensionInfo(item.data);
    data.location = chrome.developerPrivate.Location.UNKNOWN;
    item.data = data;
    await microtasksFinished();
    assertTrue(isChildVisible(item, '#source-indicator'));
    assertEquals('extensions-icons:input', icon.icon);

    data = createExtensionInfo(item.data);
    data.location = chrome.developerPrivate.Location.INSTALLED_BY_DEFAULT;
    item.data = data;
    await microtasksFinished();
    assertFalse(isChildVisible(item, '#source-indicator'));

    data = createExtensionInfo(item.data);
    data.location = chrome.developerPrivate.Location.FROM_STORE;
    data.controlledInfo = {text: 'policy'};
    item.data = data;
    await microtasksFinished();
    assertTrue(isChildVisible(item, '#source-indicator'));
    // Re-query the icon, since it was removed from the DOM when the source
    // indicator was behind an if()
    icon = item.shadowRoot!.querySelector<CrIconElement>(
        '#source-indicator cr-icon');
    assertTrue(!!icon);
    assertEquals('extensions-icons:business', icon.icon);

    data = createExtensionInfo(item.data);
    data.controlledInfo = undefined;
    item.data = data;
    await microtasksFinished();
    assertFalse(isChildVisible(item, '#source-indicator'));
  });

  test('EnableToggle', async () => {
    assertFalse(item.$.enableToggle.disabled);

    // Test case where user does not have permission.
    let data = createExtensionInfo(item.data);
    data.userMayModify = false;
    item.data = data;
    await microtasksFinished();
    assertTrue(item.$.enableToggle.disabled);
    // Reset state.
    data = createExtensionInfo(item.data);
    data.userMayModify = true;
    item.data = data;
    await microtasksFinished();

    // Test case of a blocklisted extension.
    data = createExtensionInfo(item.data);
    data.state = chrome.developerPrivate.ExtensionState.BLOCKLISTED;
    item.data = data;
    await microtasksFinished();
    assertTrue(item.$.enableToggle.disabled);
    // Reset state.
    data = createExtensionInfo(item.data);
    data.state = chrome.developerPrivate.ExtensionState.ENABLED;
    item.data = data;
    await microtasksFinished();

    // This section tests that the enable toggle is visible but disabled
    // when disableReasons.blockedByPolicy is true. This test prevents a
    // regression to crbug/1003014.
    data = createExtensionInfo(item.data);
    data.disableReasons.blockedByPolicy = true;
    item.data = data;
    await microtasksFinished();
    testVisible(item, '#enableToggle', true);
    assertTrue(item.$.enableToggle.disabled);

    data = createExtensionInfo(item.data);
    data.disableReasons.blockedByPolicy = false;
    data.disableReasons.publishedInStoreRequired = true;
    item.data = data;
    await microtasksFinished();
    testVisible(item, '#enableToggle', true);
    assertTrue(item.$.enableToggle.disabled);

    data = createExtensionInfo(item.data);
    data.disableReasons.publishedInStoreRequired = false;
    item.data = data;
    await microtasksFinished();

    testVisible(item, '#parentDisabledPermissionsToolTip', false);
    data = createExtensionInfo(item.data);
    data.disableReasons.parentDisabledPermissions = true;
    item.data = data;
    await microtasksFinished();
    testVisible(item, '#enableToggle', true);
    assertFalse(item.$.enableToggle.disabled);
    testVisible(item, '#parentDisabledPermissionsToolTip', true);

    data = createExtensionInfo(item.data);
    data.disableReasons.parentDisabledPermissions = false;
    data.disableReasons.custodianApprovalRequired = true;
    item.data = data;
    await microtasksFinished();
    testVisible(item, '#enableToggle', true);
    assertFalse(item.$.enableToggle.disabled);

    // MV2 deprecation tests cases.
    // Extension toggle is visible and enabled when MV2 experiment is 'disable
    // with re-enable' and extension is disabled due to unsupported manifest
    // version.
    data = createExtensionInfo(item.data);
    data.disableReasons.custodianApprovalRequired = false;
    data.disableReasons.unsupportedManifestVersion = true;
    item.mv2ExperimentStage = Mv2ExperimentStage.DISABLE_WITH_REENABLE;
    item.data = data;
    await microtasksFinished();
    testVisible(item, '#enableToggle', true);
    assertFalse(item.$.enableToggle.disabled);

    // Extension toggle is visible and disabled when MV2 experiment is
    // 'unsupported' and extension is disabled due to unsupported manifest
    // version.
    item.mv2ExperimentStage = Mv2ExperimentStage.UNSUPPORTED;
    await microtasksFinished();
    testVisible(item, '#enableToggle', true);
    assertTrue(item.$.enableToggle.disabled);
  });

  test('RemoveButton', async () => {
    assertFalse(item.$.removeButton.hidden);
    const data = createExtensionInfo(item.data);
    data.mustRemainInstalled = true;
    item.data = data;
    await microtasksFinished();
    assertTrue(item.$.removeButton.hidden);
  });

  test('HtmlInName', async () => {
    const name = '<HTML> in the name!';
    const data = createExtensionInfo(item.data);
    data.name = name;
    item.data = data;
    await microtasksFinished();
    assertEquals(name, item.$.name.textContent!.trim());
    // "Related to $1" is IDS_MD_EXTENSIONS_EXTENSION_A11Y_ASSOCIATION.
    assertEquals(
        `Related to ${name}`, item.$.a11yAssociation.textContent!.trim());
  });

  test('RepairButton', async () => {
    // For most extensions, the "repair" button should be displayed if the
    // extension is detected as corrupted.
    testVisible(item, '#repair-button', false);
    let data = createExtensionInfo(item.data);
    data.disableReasons.corruptInstall = true;
    item.data = data;
    await microtasksFinished();
    testVisible(item, '#repair-button', true);

    data = createExtensionInfo(item.data);
    data.disableReasons.corruptInstall = false;
    item.data = data;
    await microtasksFinished();
    testVisible(item, '#repair-button', false);

    // However, the user isn't allowed to initiate a repair for extensions they
    // aren't allowed to modify, so the button shouldn't be visible.
    data = createExtensionInfo(item.data);
    data.userMayModify = false;
    data.disableReasons.corruptInstall = true;
    item.data = data;
    await microtasksFinished();
    testVisible(item, '#repair-button', false);
  });

  test('InspectableViewSortOrder', async () => {
    function getUrl(path: string) {
      return `chrome-extension://${extensionData.id}/${path}`;
    }
    const data = createExtensionInfo(item.data);
    data.views = [
      {
        renderProcessId: 0,
        renderViewId: 0,
        incognito: false,
        isIframe: false,
        type: chrome.developerPrivate.ViewType.EXTENSION_POPUP,
        url: getUrl('popup.html'),
      },
      {
        renderProcessId: 0,
        renderViewId: 0,
        incognito: false,
        isIframe: false,
        type: chrome.developerPrivate.ViewType.EXTENSION_BACKGROUND_PAGE,
        url: getUrl('_generated_background_page.html'),
      },
      {
        renderProcessId: 0,
        renderViewId: 0,
        incognito: false,
        isIframe: false,
        type: chrome.developerPrivate.ViewType
                  .EXTENSION_SERVICE_WORKER_BACKGROUND,
        url: getUrl('sw.js'),
      },
    ];
    item.data = data;
    item.inDevMode = true;
    await microtasksFinished();

    // Check that when multiple views are available, the service worker is
    // sorted first.
    assertEquals(
        'service worker,',
        item.shadowRoot!
            .querySelector<HTMLElement>(
                '#inspect-views a:first-of-type')!.textContent!.trim());
  });

  // Test that the correct tooltip text is shown when the enable toggle is
  // hovered over, depending on if the extension is enabled/disabled and its
  // permissions.
  test('EnableExtensionToggleTooltips', async() => {
    const crTooltip =
        item.shadowRoot!.querySelector<HTMLElement>('#enable-toggle-tooltip')!;
    testVisible(item, '#enable-toggle-tooltip', false);

    item.$.enableToggle.dispatchEvent(
        new CustomEvent('pointerenter', {bubbles: true, composed: true}));
    await microtasksFinished();
    testVisible(item, '#enable-toggle-tooltip', true);
    assertEquals(
        loadTimeData.getString('enableToggleTooltipEnabled'),
        crTooltip.textContent!.trim());

    let data = createExtensionInfo(item.data);
    data.permissions = {
      simplePermissions: [{message: 'activeTab', submessages: []}],
      canAccessSiteData: true,
    };
    item.data = data;
    await microtasksFinished();
    assertEquals(
        loadTimeData.getString('enableToggleTooltipEnabledWithSiteAccess'),
        crTooltip.textContent!.trim());

    data = createExtensionInfo(item.data);
    data.state = chrome.developerPrivate.ExtensionState.DISABLED;
    item.data = data;
    await microtasksFinished();
    assertEquals(
        loadTimeData.getString('enableToggleTooltipDisabled'),
        crTooltip.textContent!.trim());
  });
});
