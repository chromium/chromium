// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extensions-detail-view. */

import type {CrCheckboxElement, ExtensionsDetailViewElement, ExtensionsToggleRowElement} from 'chrome://extensions/extensions.js';
import {Mv2ExperimentStage, navigation, Page} from 'chrome://extensions/extensions.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';

import {TestService} from './test_service.js';
import {createExtensionInfo, MockItemDelegate, testVisible} from './test_util.js';

suite('ExtensionDetailViewTest', function() {
  /** Extension item created before each test. */
  let item: ExtensionsDetailViewElement;

  /** Backing extension data for the item. */
  let extensionData: chrome.developerPrivate.ExtensionInfo;

  let mockDelegate: MockItemDelegate;

  // Initialize an extension item before each test.
  setup(function() {
    setupElement();
  });

  function setupElement() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    extensionData = createExtensionInfo({
      incognitoAccess: {isEnabled: true, isActive: false},
      fileAccess: {isEnabled: true, isActive: false},
      errorCollection: {isEnabled: true, isActive: false},
    });
    mockDelegate = new MockItemDelegate();
    item = document.createElement('extensions-detail-view');
    item.set('data', extensionData);
    item.set('delegate', mockDelegate);
    item.set('inDevMode', false);
    item.set('incognitoAvailable', true);
    item.set('showActivityLog', false);
    item.set('enableEnhancedSiteControls', false);
    document.body.appendChild(item);
    const toastManager = document.createElement('cr-toast-manager');
    document.body.appendChild(toastManager);
  }

  test('Layout', function() {
    flush();

    const testIsVisible: (selector: string) => boolean =
        isChildVisible.bind(null, item);
    assertTrue(testIsVisible('#closeButton'));
    assertTrue(testIsVisible('#icon'));
    assertFalse(testIsVisible('#extensionsOptions'));
    assertTrue(
        item.$.description.textContent!.indexOf('This is an extension') !== -1);
    assertTrue(testIsVisible('#siteSettings'));

    // Check the checkboxes visibility and state. They should be visible
    // only if the associated option is enabled, and checked if the
    // associated option is active.
    const accessOptions = [
      {key: 'incognitoAccess', id: '#allow-incognito'},
      {key: 'fileAccess', id: '#allow-on-file-urls'},
      {key: 'errorCollection', id: '#collect-errors'},
    ];
    const isChecked = (id: string) =>
        item.shadowRoot!.querySelector<CrCheckboxElement>(id)!.checked;
    for (const option of accessOptions) {
      assertTrue(isChildVisible(item, option.id));
      assertFalse(isChecked(option.id), option.id);
      item.set('data.' + option.key + '.isEnabled', false);
      flush();
      assertFalse(isChildVisible(item, option.id));
      item.set('data.' + option.key + '.isEnabled', true);
      item.set('data.' + option.key + '.isActive', true);
      flush();
      assertTrue(isChildVisible(item, option.id));
      assertTrue(isChecked(option.id));
    }

    assertFalse(testIsVisible('#dependent-extensions-list'));
    item.set(
        'data.dependentExtensions',
        [{id: 'aaa', name: 'Dependent1'}, {id: 'bbb', name: 'Dependent2'}]);
    flush();
    assertTrue(testIsVisible('#dependent-extensions-list'));
    assertEquals(
        2,
        item.shadowRoot!.querySelector('#dependent-extensions-list')!
            .querySelectorAll('li')
            .length);

    assertFalse(testIsVisible('#permissions-list'));
    assertFalse(testIsVisible('#host-access'));
    assertFalse(testIsVisible('extensions-runtime-host-permissions'));

    assertTrue(testIsVisible('#no-permissions'));
    item.set('data.permissions', {
      simplePermissions: ['Permission 1', 'Permission 2'],
      canAccessSiteData: false,
    });
    flush();
    assertTrue(testIsVisible('#permissions-list'));
    assertEquals(
        2,
        item.shadowRoot!.querySelector('#permissions-list')!
            .querySelectorAll('li:not([hidden])')
            .length);
    assertFalse(testIsVisible('#no-permissions'));
    assertFalse(testIsVisible('#host-access'));
    assertFalse(testIsVisible('extensions-runtime-host-permissions'));
    // Reset state.
    item.set('data.dependentExtensions', []);
    item.set(
        'data.permissions', {simplePermissions: [], canAccessSiteData: false});
    flush();

    const optionsUrl =
        'chrome-extension://' + extensionData.id + '/options.html';
    item.set('data.optionsPage', {openInTab: true, url: optionsUrl});
    assertTrue(testIsVisible('#extensionsOptions'));

    assertFalse(testIsVisible('#extensionsActivityLogLink'));
    item.set('showActivityLog', true);
    flush();
    assertTrue(testIsVisible('#extensionsActivityLogLink'));

    item.set('data.manifestHomePageUrl', 'http://example.com');
    flush();
    assertTrue(testIsVisible('#extensionWebsite'));
    item.set('data.manifestHomePageUrl', '');
    flush();
    assertFalse(testIsVisible('#extensionWebsite'));

    item.set('data.webStoreUrl', 'http://example.com');
    flush();
    assertTrue(testIsVisible('#viewInStore'));
    item.set('data.webStoreUrl', '');
    flush();
    assertFalse(testIsVisible('#viewInStore'));

    assertFalse(testIsVisible('#id-section'));
    assertFalse(testIsVisible('#inspectable-views'));
    assertFalse(testIsVisible('#dev-reload-button'));

    item.set('inDevMode', true);
    flush();
    assertTrue(testIsVisible('#id-section'));
    assertTrue(testIsVisible('#inspectable-views'));

    assertTrue(item.data.incognitoAccess.isEnabled);
    item.set('incognitoAvailable', false);
    flush();
    assertFalse(testIsVisible('#allow-incognito'));

    item.set('incognitoAvailable', true);
    flush();
    assertTrue(testIsVisible('#allow-incognito'));

    // Ensure that the "Extension options" button is disabled when the item
    // itself is disabled.
    const extensionOptions = item.$.extensionsOptions;
    assertFalse(extensionOptions.disabled);
    item.set('data.state', chrome.developerPrivate.ExtensionState.DISABLED);
    flush();
    assertTrue(extensionOptions.disabled);

    assertFalse(testIsVisible('.warning-icon'));
    item.set('data.runtimeWarnings', ['Dummy warning']);
    flush();
    assertTrue(testIsVisible('.warning-icon'));

    assertTrue(testIsVisible('#enableToggle'));
    assertFalse(testIsVisible('#terminated-reload-button'));
    item.set('data.state', chrome.developerPrivate.ExtensionState.TERMINATED);
    flush();
    assertFalse(testIsVisible('#enableToggle'));
    assertTrue(testIsVisible('#terminated-reload-button'));

    // Ensure that the runtime warning reload button is not visible if there
    // are runtime warnings and the extension is terminated.
    item.set('data.runtimeWarnings', ['Dummy warning']);
    flush();
    assertFalse(testIsVisible('#warnings-reload-button'));
    item.set('data.runtimeWarnings', []);

    // Reset item state back to DISABLED.
    item.set('data.state', chrome.developerPrivate.ExtensionState.DISABLED);
    flush();

    // Ensure that without runtimeHostPermissions data, the sections are
    // hidden.
    assertTrue(testIsVisible('#no-site-access'));
    assertFalse(testIsVisible('extensions-runtime-host-permissions'));
    assertFalse(testIsVisible('extensions-host-permissions-toggle-list'));

    // Adding any runtime host permissions should result in the runtime host
    // controls becoming visible.
    const allSitesPermissions = {
      simplePermissions: [],
      runtimeHostPermissions: {
        hosts: [{granted: false, host: '<all_urls>'}],
        hasAllHosts: true,
        hostAccess: chrome.developerPrivate.HostAccess.ON_CLICK,
      },
      canAccessSiteData: true,
    };
    item.set('data.permissions', allSitesPermissions);
    flush();
    assertFalse(testIsVisible('#no-site-access'));
    assertTrue(testIsVisible('extensions-runtime-host-permissions'));
    assertFalse(testIsVisible('extensions-host-permissions-toggle-list'));

    const someSitesPermissions = {
      simplePermissions: [],
      runtimeHostPermissions: {
        hosts: [
          {granted: true, host: 'https://chromium.org/*'},
          {granted: false, host: 'https://example.com/*'},
        ],
        hasAllHosts: false,
        hostAccess: chrome.developerPrivate.HostAccess.ON_SPECIFIC_SITES,
      },
      canAccessSiteData: true,
    };
    item.set('data.permissions', someSitesPermissions);
    flush();
    assertFalse(testIsVisible('#no-site-access'));
    assertFalse(testIsVisible('extensions-runtime-host-permissions'));
    assertTrue(testIsVisible('extensions-host-permissions-toggle-list'));
  });

  test('LayoutSource', function() {
    item.set('data.location', 'FROM_STORE');
    flush();
    assertEquals('Chrome Web Store', item.$.source.textContent!.trim());
    assertFalse(isChildVisible(item, '#load-path'));

    item.set('data.location', 'THIRD_PARTY');
    flush();
    assertEquals('Added by a third-party', item.$.source.textContent!.trim());
    assertFalse(isChildVisible(item, '#load-path'));

    item.set('data.location', 'INSTALLED_BY_DEFAULT');
    flush();
    assertEquals('Installed by default', item.$.source.textContent!.trim());
    assertFalse(isChildVisible(item, '#load-path'));

    item.set('data.location', 'UNPACKED');
    item.set('data.prettifiedPath', 'foo/bar/baz/');
    flush();
    assertEquals('Unpacked extension', item.$.source.textContent!.trim());
    // Test whether the load path is displayed for unpacked extensions.
    assertTrue(isChildVisible(item, '#load-path'));

    item.set('data.location', 'UNKNOWN');
    item.set('data.prettifiedPath', '');
    // |locationText| is expected to always be set if location is UNKNOWN.
    item.set('data.locationText', 'Foo');
    flush();
    assertEquals('Foo', item.$.source.textContent!.trim());
    assertFalse(isChildVisible(item, '#load-path'));
  });

  test('ElementVisibilityReloadButton', function() {
    item.set('inDevMode', true);

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

  /** Tests that the reload button properly fires the load-error event. */
  test('FailedReloadFiresLoadError', async function() {
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

  test('SupervisedUserDisableReasons', function() {
    flush();
    const toggle = item.$.enableToggle;
    const tooltip = item.$.parentDisabledPermissionsToolTip;
    assertTrue(isVisible(toggle));
    assertFalse(isVisible(tooltip));

    // This section tests that the enable toggle is visible but disabled
    // when disableReasons.blockedByPolicy is true. This test prevents a
    // regression to crbug/1003014.
    item.set('data.disableReasons.blockedByPolicy', true);
    flush();
    assertTrue(isVisible(toggle));
    assertTrue(toggle.disabled);
    item.set('data.disableReasons.blockedByPolicy', false);
    flush();

    item.set('data.disableReasons.parentDisabledPermissions', true);
    flush();
    assertTrue(isVisible(toggle));
    assertFalse(toggle.disabled);
    assertTrue(isVisible(tooltip));
    item.set('data.disableReasons.parentDisabledPermissions', false);
    flush();

    item.set('data.disableReasons.custodianApprovalRequired', true);
    flush();
    assertTrue(isVisible(toggle));
    assertFalse(toggle.disabled);
    item.set('data.disableReasons.custodianApprovalRequired', false);
    flush();
  });

  test('MV2DeprecationDisabledExtension', function() {
    const toggle = item.$.enableToggle;

    // Extension toggle is visible and enabled for MV2 experiment is 'disable
    // with re-enable' and extension is disabled due to unsupported manifest
    // version.
    item.set('mv2ExperimentStage_', Mv2ExperimentStage.DISABLE_WITH_REENABLE);
    item.set('data.disableReasons.unsupportedManifestVersion', true);
    flush();
    assertTrue(isVisible(toggle));
    assertFalse(toggle.disabled);

    // Extension toggle is visible and disabled when MV2 experiment is
    // 'unsupported' and extension is disabled due to unsupported manifest
    // version.
    item.set('mv2ExperimentStage_', Mv2ExperimentStage.UNSUPPORTED);
    item.set('data.disableReasons.unsupportedManifestVersion', true);
    flush();
    assertTrue(isVisible(toggle));
    assertTrue(toggle.disabled);
  });

  test('ClickableElements', async function() {
    const optionsUrl =
        'chrome-extension://' + extensionData.id + '/options.html';
    item.set('data.optionsPage', {openInTab: true, url: optionsUrl});
    item.set('data.prettifiedPath', 'foo/bar/baz/');
    item.set('showActivityLog', true);
    flush();

    let currentPage = null;
    navigation.addListener(newPage => {
      currentPage = newPage;
    });

    // Even though the command line flag is not set for activity log, we
    // still expect to navigate to it after clicking the link as the logic
    // to redirect the page back to the details view is in manager.js. Since
    // this behavior does not happen in the testing environment, we test the
    // behavior in manager_test.js.
    item.$.extensionsActivityLogLink.click();
    assertDeepEquals(
        currentPage, {page: Page.ACTIVITY_LOG, extensionId: extensionData.id});

    // Reset current page and test delegate calls.
    navigation.navigateTo({page: Page.DETAILS, extensionId: extensionData.id});
    currentPage = null;

    await mockDelegate.testClickingCalls(
        item.shadowRoot!
            .querySelector<ExtensionsToggleRowElement>(
                '#allow-incognito')!.getLabel(),
        'setItemAllowedIncognito', [extensionData.id, true]);
    await mockDelegate.testClickingCalls(
        item.shadowRoot!
            .querySelector<ExtensionsToggleRowElement>(
                '#allow-on-file-urls')!.getLabel(),
        'setItemAllowedOnFileUrls', [extensionData.id, true]);
    await mockDelegate.testClickingCalls(
        item.shadowRoot!
            .querySelector<ExtensionsToggleRowElement>(
                '#collect-errors')!.getLabel(),
        'setItemCollectsErrors', [extensionData.id, true]);
    await mockDelegate.testClickingCalls(
        item.$.extensionsOptions, 'showItemOptionsPage', [extensionData]);
    await mockDelegate.testClickingCalls(
        item.shadowRoot!.querySelector('#remove-extension')!, 'deleteItem',
        [extensionData.id]);
    await mockDelegate.testClickingCalls(
        item.shadowRoot!.querySelector('#load-path > a[is=\'action-link\']')!,
        'showInFolder', [extensionData.id]);
    await mockDelegate.testClickingCalls(
        item.shadowRoot!.querySelector('#warnings-reload-button')!,
        'reloadItem', [extensionData.id], Promise.resolve());

    // We need to wait for isReloading_ to be set to false, which happens
    // slightly asynchronously.
    await new Promise((resolve) => setTimeout(resolve, 0));

    // Terminate the extension so the reload button appears.
    item.set('data.state', chrome.developerPrivate.ExtensionState.TERMINATED);
    flush();
    await mockDelegate.testClickingCalls(
        item.shadowRoot!.querySelector('#terminated-reload-button')!,
        'reloadItem', [extensionData.id], Promise.resolve());
  });

  test('Indicator', function() {
    const indicator = item.shadowRoot!.querySelector('cr-tooltip-icon')!;
    assertTrue(indicator.hidden);
    item.set('data.controlledInfo', {text: 'policy'});
    flush();
    assertFalse(indicator.hidden);
  });

  test('Warnings', function() {
    function testWarningVisible(id: string, expectVisible: boolean): void {
      const f: (arg: boolean) => void =
          expectVisible ? assertTrue : assertFalse;
      f(isChildVisible(item, id));
    }

    testWarningVisible('#runtime-warnings', false);
    testWarningVisible('#corrupted-warning', false);
    testWarningVisible('#suspicious-warning', false);
    testWarningVisible('#blocklisted-warning', false);
    testWarningVisible('#update-required-warning', false);
    testWarningVisible('#published-in-store-required-warning', false);

    item.set('data.runtimeWarnings', ['Dummy warning']);
    flush();
    testWarningVisible('#runtime-warnings', true);
    testWarningVisible('#corrupted-warning', false);
    testWarningVisible('#suspicious-warning', false);
    testWarningVisible('#blocklisted-warning', false);
    testWarningVisible('#update-required-warning', false);
    testWarningVisible('#published-in-store-required-warning', false);

    item.set('data.disableReasons.corruptInstall', true);
    flush();
    testWarningVisible('#runtime-warnings', true);
    testWarningVisible('#corrupted-warning', true);
    testWarningVisible('#suspicious-warning', false);
    testWarningVisible('#blocklisted-warning', false);
    testWarningVisible('#update-required-warning', false);
    testWarningVisible('#published-in-store-required-warning', false);
    const testIsVisible = isChildVisible.bind(null, item);
    assertTrue(testIsVisible('#enableToggle'));

    item.set('data.disableReasons.suspiciousInstall', true);
    flush();
    testWarningVisible('#runtime-warnings', true);
    testWarningVisible('#corrupted-warning', true);
    testWarningVisible('#suspicious-warning', true);
    testWarningVisible('#blocklisted-warning', false);
    testWarningVisible('#update-required-warning', false);
    testWarningVisible('#published-in-store-required-warning', false);

    item.set('data.blocklistText', 'This item is blocklisted');
    flush();
    testWarningVisible('#runtime-warnings', true);
    testWarningVisible('#corrupted-warning', true);
    testWarningVisible('#suspicious-warning', true);
    testWarningVisible('#blocklisted-warning', true);
    testWarningVisible('#update-required-warning', false);
    testWarningVisible('#published-in-store-required-warning', false);

    item.set('data.blocklistText', null);
    flush();
    testWarningVisible('#runtime-warnings', true);
    testWarningVisible('#corrupted-warning', true);
    testWarningVisible('#suspicious-warning', true);
    testWarningVisible('#blocklisted-warning', false);
    testWarningVisible('#update-required-warning', false);
    testWarningVisible('#published-in-store-required-warning', false);

    item.set('data.disableReasons.updateRequired', true);
    flush();
    testWarningVisible('#runtime-warnings', true);
    testWarningVisible('#corrupted-warning', true);
    testWarningVisible('#suspicious-warning', true);
    testWarningVisible('#blocklisted-warning', false);
    testWarningVisible('#update-required-warning', true);
    testWarningVisible('#published-in-store-required-warning', false);

    item.set('data.disableReasons.publishedInStoreRequired', true);
    flush();
    testWarningVisible('#runtime-warnings', true);
    testWarningVisible('#corrupted-warning', true);
    testWarningVisible('#suspicious-warning', true);
    testWarningVisible('#blocklisted-warning', false);
    testWarningVisible('#update-required-warning', true);
    testWarningVisible('#published-in-store-required-warning', true);

    item.set('data.runtimeWarnings', []);
    item.set('data.disableReasons.corruptInstall', false);
    item.set('data.disableReasons.suspiciousInstall', false);
    item.set('data.disableReasons.updateRequired', false);
    item.set('data.disableReasons.publishedInStoreRequired', false);
    flush();
    testWarningVisible('#runtime-warnings', false);
    testWarningVisible('#corrupted-warning', false);
    testWarningVisible('#suspicious-warning', false);
    testWarningVisible('#blocklisted-warning', false);
    testWarningVisible('#update-required-warning', false);
    testWarningVisible('#published-in-store-required-warning', false);

    item.set('data.showSafeBrowsingAllowlistWarning', true);
    flush();
    testWarningVisible('#runtime-warnings', false);
    testWarningVisible('#corrupted-warning', false);
    testWarningVisible('#suspicious-warning', false);
    testWarningVisible('#blocklisted-warning', false);
    testWarningVisible('#update-required-warning', false);
    testWarningVisible('#published-in-store-required-warning', false);
    testWarningVisible('#allowlist-warning', true);

    item.set('data.disableReasons.suspiciousInstall', true);
    flush();
    testWarningVisible('#runtime-warnings', false);
    testWarningVisible('#corrupted-warning', false);
    testWarningVisible('#suspicious-warning', true);
    testWarningVisible('#blocklisted-warning', false);
    testWarningVisible('#update-required-warning', false);
    testWarningVisible('#published-in-store-required-warning', false);
    testWarningVisible('#allowlist-warning', true);

    // Test that the allowlist warning is not shown when there is already a
    // blocklist message. It would be redundant since all blocklisted extension
    // are necessarily not included in the Safe Browsing allowlist.
    item.set('data.blocklistText', 'This item is blocklisted');
    flush();
    testWarningVisible('#runtime-warnings', false);
    testWarningVisible('#corrupted-warning', false);
    testWarningVisible('#suspicious-warning', true);
    testWarningVisible('#blocklisted-warning', true);
    testWarningVisible('#update-required-warning', false);
    testWarningVisible('#published-in-store-required-warning', false);
    testWarningVisible('#allowlist-warning', false);
  });

  test('NoSiteAccessWithEnhancedSiteControls', function() {
    const testIsVisible = isChildVisible.bind(null, item);

    // Ensure that if the enableEnhancedSiteControls flag is enabled, then
    // the no site access message is in the permissions section and not in
    // the site access section.
    item.set('data.dependentExtensions', []);
    item.set(
        'data.permissions', {simplePermissions: [], canAccessSiteData: false});
    item.enableEnhancedSiteControls = true;
    flush();

    assertTrue(testIsVisible('#no-permissions'));
    assertTrue(item.shadowRoot!.querySelector<HTMLElement>('#no-permissions')!
                   .textContent!.includes(loadTimeData.getString(
                       'itemPermissionsAndSiteAccessEmpty')));
    assertFalse(testIsVisible('#no-site-access'));

    item.set('data.permissions', {
      simplePermissions: ['Permission 1', 'Permission 2'],
      canAccessSiteData: false,
    });
    flush();

    // The permissions list should contain the above 2 permissions as well
    // as an item for no additional site permissions.
    assertTrue(testIsVisible('#permissions-list'));
    assertEquals(
        3,
        item.shadowRoot!.querySelector('#permissions-list')!
            .querySelectorAll('li:not([hidden])')
            .length);
    assertFalse(testIsVisible('#no-permissions'));
    assertTrue(testIsVisible('#permissions-list li:last-of-type'));
  });

  test('InspectableViewSortOrder', function() {
    function getUrl(path: string) {
      return `chrome-extension://${extensionData.id}/${path}`;
    }
    item.set('data.views', [
      {
        type: chrome.developerPrivate.ViewType.EXTENSION_BACKGROUND_PAGE,
        url: getUrl('_generated_background_page.html'),
      },
      {
        type: chrome.developerPrivate.ViewType
                  .EXTENSION_SERVICE_WORKER_BACKGROUND,
        url: getUrl('sw.js'),
      },
      {
        type: chrome.developerPrivate.ViewType.EXTENSION_POPUP,
        url: getUrl('popup.html'),
      },
    ]);
    item.set('inDevMode', true);
    flush();

    const orderedListItems =
        Array
            .from(item.shadowRoot!.querySelectorAll<HTMLElement>(
                '.inspectable-view'))
            .map(e => e.textContent!.trim());

    assertDeepEquals(
        ['service worker', 'background page', 'popup.html'], orderedListItems);
  });

  test('ShowAccessRequestsInToolbar', async function() {
    const testIsVisible = isChildVisible.bind(null, item);

    const allSitesPermissions = {
      simplePermissions: [],
      runtimeHostPermissions: {
        hosts: [{granted: false, host: '<all_urls>'}],
        hasAllHosts: true,
        hostAccess: chrome.developerPrivate.HostAccess.ON_CLICK,
      },
      canAccessSiteData: true,
    };
    item.set('data.permissions', allSitesPermissions);
    item.set('data.showAccessRequestsInToolbar', true);
    flush();

    assertFalse(testIsVisible('#show-access-requests-toggle'));

    item.enableEnhancedSiteControls = true;
    flush();

    assertTrue(testIsVisible('#show-access-requests-toggle'));
    assertTrue(item.shadowRoot!
                   .querySelector<ExtensionsToggleRowElement>(
                       '#show-access-requests-toggle')!.checked);

    await mockDelegate.testClickingCalls(
        item.shadowRoot!
            .querySelector<ExtensionsToggleRowElement>(
                '#show-access-requests-toggle')!.getLabel(),
        'setShowAccessRequestsInToolbar', [extensionData.id, false]);
  });

  test('SafetyCheckWarning', function() {
    // Ensure that the SafetyCheckWarningContainer is not visible
    // before enabling the feature.
    assertFalse(isVisible(
        item.shadowRoot!.querySelector('#safetyCheckWarningContainer')));
    loadTimeData.overrideValues({'safetyCheckShowReviewPanel': true});
    item.set('data.safetyCheckText', {'detailString': 'Test Message'});
    item.set('data.blocklistText', 'This item is blocklisted');  // nocheck
    flush();
    // Check to make sure the warning text is hidden due to the
    // SafetyCheckWarningContainer being shown.
    assertFalse(isVisible(
        item.shadowRoot!.querySelector('#blocklisted-warning')));  // nocheck
    const safetyWarningText =
        item.shadowRoot!.querySelector('#safetyCheckWarningContainer');
    assertTrue(!!safetyWarningText);
    assertTrue(isVisible(safetyWarningText));
    assertTrue(safetyWarningText!.textContent!.includes('Test Message'));
  });

  test('Mv2DeprecationMessage_None', function() {
    // Message is hidden for experiment on 'none' stage.
    loadTimeData.overrideValues({MV2ExperimentStage: Mv2ExperimentStage.NONE});
    setupElement();
    flush();
    testVisible(item, '#mv2DeprecationMessage', false);
  });

  test('Mv2DeprecationMessage_Warning', async function() {
    // Message is hidden for experiment on 'warning' stage when extension is
    // not affected by the MV2 deprecation.
    loadTimeData.overrideValues(
        {MV2ExperimentStage: Mv2ExperimentStage.WARNING});
    setupElement();
    flush();
    testVisible(item, '#mv2DeprecationMessage', false);

    // Message is visible for experiment on 'warning' stage when extension is
    // affected by the MV2 deprecation.
    item.set('data.isAffectedByMV2Deprecation', true);
    flush();
    testVisible(item, '#mv2DeprecationMessage', true);

    // Find alternative button is hidden when the extension doesn't have a
    // recommendations url.
    const findAlternativeButton =
        item.shadowRoot!.querySelector<HTMLElement>('#mv2DeprecationMessage')!
            .querySelector<HTMLButtonElement>('.find-alternative-button');
    assertTrue(!!findAlternativeButton);
    assertFalse(isVisible(findAlternativeButton));

    // Remove button is always hidden.
    const removeButton =
        item.shadowRoot!.querySelector<HTMLElement>('#mv2DeprecationMessage')!
            .querySelector<HTMLButtonElement>('.remove-button');
    assertTrue(!!removeButton);
    assertFalse(isVisible(removeButton));

    // Action menu is always hidden.
    const actionMenu =
        item.shadowRoot!.querySelector<HTMLElement>('#mv2DeprecationMessage')!
            .querySelector<HTMLButtonElement>('#actionMenu');
    assertTrue(!!actionMenu);
    assertFalse(isVisible(actionMenu));

    // Add a recommendations url to the extension.
    const id = 'a'.repeat(32);
    const recommendationsUrl =
        `https://chromewebstore.google.com/detail/${id}` +
        `/related-recommendations`;
    item.set('data.recommendationsUrl', recommendationsUrl);
    flush();

    // Find alternative button is visible when the extension has a
    // recommendations url.
    assertTrue(isVisible(findAlternativeButton));

    // Click on the find alternative button, and verify it triggered the
    // correct delegate call.
    await mockDelegate.testClickingCalls(
        findAlternativeButton, 'openUrl', [recommendationsUrl]);
  });

  test('Mv2DeprecationMessage_DisableWithReEnable_Visbility', async function() {
    // Message is hidden for experiment on 'disable with re-enable' stage when
    // extension is not affected by the MV2 deprecation.
    loadTimeData.overrideValues(
        {MV2ExperimentStage: Mv2ExperimentStage.DISABLE_WITH_REENABLE});
    setupElement();
    flush();
    testVisible(item, '#mv2DeprecationMessage', false);

    // Message is hidden for experiment on stage 2 (disable with re-enable)
    // when extension is affected by the MV2 deprecation but it's not disabled
    // due to unsupported manifest version.
    // Note: This can happen when the user chose to re-enable a MV2 disabled
    // extension.
    item.set('data.isAffectedByMV2Deprecation', true);
    item.set('data.disableReasons.unsupportedManifestVersion', false);
    flush();
    testVisible(item, '#mv2DeprecationMessage', false);

    // Message is visible for experiment on stage 2 (disable with re-enable)
    // when extension is affected by the MV2 deprecation and extension is
    // disabled due to unsupported manifest version.
    item.set('data.disableReasons.unsupportedManifestVersion', true);
    flush();
    testVisible(item, '#mv2DeprecationMessage', true);

    // Message is hidden for experiment on stage 2 (disable with re-enable)
    // when extension is affected by the MV2 deprecation and has been
    // acknowledged.
    item.set('data.didAcknowledgeMV2DeprecationNotice', true);
    flush();
    testVisible(item, '#mv2DeprecationMessage', false);
  });

  test('Mv2DeprecationMessage_DisableWithReEnable_Content', async function() {
    // Show the message for experiment on 'disable with re-enable' stage by
    // setting the corresponding properties.
    loadTimeData.overrideValues(
        {MV2ExperimentStage: Mv2ExperimentStage.DISABLE_WITH_REENABLE});
    setupElement();
    item.set('data.isAffectedByMV2Deprecation', true);
    item.set('data.disableReasons.unsupportedManifestVersion', true);
    flush();
    testVisible(item, '#mv2DeprecationMessage', true);

    // Find alternative button is always hidden.
    const findAlternativeButton =
        item.shadowRoot!.querySelector<HTMLElement>('#mv2DeprecationMessage')!
            .querySelector<HTMLButtonElement>('.find-alternative-button');
    assertTrue(!!findAlternativeButton);
    assertFalse(isVisible(findAlternativeButton));

    // Remove button is hidden if extension must remain installed.
    item.set('data.mustRemainInstalled', true);
    flush();
    const removeButton =
        item.shadowRoot!.querySelector<HTMLElement>('#mv2DeprecationMessage')!
            .querySelector<HTMLButtonElement>('.remove-button');
    assertTrue(!!removeButton);
    assertFalse(isVisible(removeButton));

    // Remove button is visible if extension doesn't need to remain installed.
    item.set('data.mustRemainInstalled', false);
    flush();
    assertTrue(isVisible(removeButton));

    // Click on the remove button, and verify it triggered the correct delegate
    // call.
    await mockDelegate.testClickingCalls(
        removeButton, 'deleteItem', [extensionData.id]);

    // Action menu is always visible.
    const actionMenu =
        item.shadowRoot!.querySelector<HTMLElement>('#mv2DeprecationMessage')!
            .querySelector<HTMLButtonElement>('#actionMenuButton');
    assertTrue(!!actionMenu);
    assertTrue(isVisible(actionMenu));

    // Open the action menu to verify its items.
    actionMenu.click();

    // Find alternative action is not visible when the extension doesn't have a
    // recommendations url.
    const findAlternativeAction =
        item.shadowRoot!.querySelector<HTMLElement>('#findAlternativeAction');
    assertTrue(!!findAlternativeAction);
    assertFalse(isVisible(findAlternativeAction));

    // Add a recommendations url to the extension.
    const id = 'a'.repeat(32);
    const recommendationsUrl =
        `https://chromewebstore.google.com/detail/${id}` +
        `/related-recommendations`;
    item.set('data.recommendationsUrl', recommendationsUrl);
    flush();

    // Find alternative action is visible when the extension has a
    // recommendations url.
    assertTrue(isVisible(findAlternativeAction));

    // Click on the find alternative action, and verify it triggered the
    // correct delegate call.
    await mockDelegate.testClickingCalls(
        findAlternativeAction, 'openUrl', [recommendationsUrl]);

    // Keep action is always visible.
    actionMenu.click();
    const keepAction =
        item.shadowRoot!.querySelector<HTMLElement>('#keepAction');
    assertTrue(!!keepAction);
    assertTrue(isVisible(keepAction));

    // Click on the keep action, and verify it triggered the correct delegate
    // call.
    await mockDelegate.testClickingCalls(
        keepAction, 'dismissMv2DeprecationNoticeForExtension', [id]);
  });

  test('Mv2DeprecationMessage_Unsupported_Visbility', async function() {
    // Message is hidden for experiment on 'unsupported' stage when extension
    // is not affected by the MV2 deprecation.
    loadTimeData.overrideValues(
        {MV2ExperimentStage: Mv2ExperimentStage.UNSUPPORTED});
    setupElement();
    flush();
    testVisible(item, '#mv2DeprecationMessage', false);

    // Message is hidden for experiment on stage 3 (unsupported) when extension
    // is affected by the MV2 deprecation but it's not disabled due to
    // unsupported manifest version.
    item.set('data.isAffectedByMV2Deprecation', true);
    item.set('data.disableReasons.unsupportedManifestVersion', false);
    flush();
    testVisible(item, '#mv2DeprecationMessage', false);

    // Message is visible for experiment on stage 3 (unsupported) when extension
    // is affected by the MV2 deprecation and extension is disabled due to
    // unsupported manifest version.
    item.set('data.disableReasons.unsupportedManifestVersion', true);
    flush();
    testVisible(item, '#mv2DeprecationMessage', true);
  });

  test('Mv2DeprecationMessage_Unsupported_Content', async function() {
    // Show the message for experiment on 'unsupported' stage by setting the
    // corresponding properties.
    loadTimeData.overrideValues(
        {MV2ExperimentStage: Mv2ExperimentStage.UNSUPPORTED});
    setupElement();
    item.set('data.isAffectedByMV2Deprecation', true);
    item.set('data.disableReasons.unsupportedManifestVersion', true);
    flush();
    testVisible(item, '#mv2DeprecationMessage', true);

    // Find alternative button is always hidden.
    const findAlternativeButton =
        item.shadowRoot!.querySelector<HTMLElement>('#mv2DeprecationMessage')!
            .querySelector<HTMLButtonElement>('.find-alternative-button');
    assertTrue(!!findAlternativeButton);
    assertFalse(isVisible(findAlternativeButton));

    // Remove button is hidden if extension must remain installed.
    item.set('data.mustRemainInstalled', true);
    flush();
    const removeButton =
        item.shadowRoot!.querySelector<HTMLElement>('#mv2DeprecationMessage')!
            .querySelector<HTMLButtonElement>('.remove-button');
    assertTrue(!!removeButton);
    assertFalse(isVisible(removeButton));

    // Remove button is visible if extension doesn't need to remain installed.
    item.set('data.mustRemainInstalled', false);
    flush();
    assertTrue(isVisible(removeButton));

    // Click on the remove button, and verify it triggered the correct delegate
    // call.
    await mockDelegate.testClickingCalls(
        removeButton, 'deleteItem', [extensionData.id]);

    // Action menu is hidden when the extension doesn't have a recommendations
    // url.
    const actionMenu =
        item.shadowRoot!.querySelector<HTMLElement>('#mv2DeprecationMessage')!
            .querySelector<HTMLButtonElement>('#actionMenuButton');
    assertTrue(!!actionMenu);
    assertFalse(isVisible(actionMenu));

    // Add a recommendations url to the extension.
    const id = 'a'.repeat(32);
    const recommendationsUrl =
        `https://chromewebstore.google.com/detail/${id}` +
        `/related-recommendations`;
    item.set('data.recommendationsUrl', recommendationsUrl);
    flush();

    // Action menu is visible when the extension has a recommendations url.
    assertTrue(isVisible(actionMenu));

    // Open the action menu to verify its items.
    actionMenu.click();

    // Find alternative action is visible.
    const findAlternativeAction =
        item.shadowRoot!.querySelector<HTMLElement>('#findAlternativeAction');
    assertTrue(!!findAlternativeAction);
    assertTrue(isVisible(findAlternativeAction));

    // Click on the find alternative action, and verify it triggered the
    // correct delegate call.
    await mockDelegate.testClickingCalls(
        findAlternativeAction, 'openUrl', [recommendationsUrl]);
  });

  test('PinnedToToolbar', async function() {
    assertFalse(
        isVisible(item.shadowRoot!.querySelector<ExtensionsToggleRowElement>(
            '#pin-to-toolbar')));

    item.set('data.pinnedToToolbar', true);
    flush();
    const itemPinnedToggle =
        item.shadowRoot!.querySelector<ExtensionsToggleRowElement>(
            '#pin-to-toolbar');
    assertTrue(isVisible(itemPinnedToggle));
    assertTrue(itemPinnedToggle!.checked);

    await mockDelegate.testClickingCalls(
        itemPinnedToggle!.getLabel(), 'setItemPinnedToToolbar',
        [extensionData.id, false]);
    flush();
    assertTrue(isVisible(itemPinnedToggle));
    assertFalse(itemPinnedToggle!.checked);
  });
});
