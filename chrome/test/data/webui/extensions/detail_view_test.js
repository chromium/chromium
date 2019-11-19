// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extensions-detail-view. */

import {navigation, Page} from 'chrome://extensions/extensions.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isVisible} from '../test_util.m.js';

import {createExtensionInfo, MockItemDelegate} from './test_util.js';

window.extension_detail_view_tests = {};
extension_detail_view_tests.suiteName = 'ExtensionDetailViewTest';
/** @enum {string} */
extension_detail_view_tests.TestNames = {
  Layout: 'layout',
  LayoutSource: 'layout of source section',
  ClickableElements: 'clickable elements',
  Indicator: 'indicator',
  Warnings: 'warnings',
};

suite(extension_detail_view_tests.suiteName, function() {
  /**
   * Extension item created before each test.
   * @type {Item}
   */
  let item;

  /**
   * Backing extension data for the item.
   * @type {chrome.developerPrivate.ExtensionInfo}
   */
  let extensionData;

  /** @type {MockItemDelegate} */
  let mockDelegate;

  // Initialize an extension item before each test.
  setup(function() {
    PolymerTest.clearBody();
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
    document.body.appendChild(item);
  });

  test(assert(extension_detail_view_tests.TestNames.Layout), function() {
    flush();

    const testIsVisible = isVisible.bind(null, item);
    expectTrue(testIsVisible('#closeButton'));
    expectTrue(testIsVisible('#icon'));
    expectTrue(testIsVisible('#enable-toggle'));
    expectFalse(testIsVisible('#extensions-options'));
    expectTrue(
        item.$.description.textContent.indexOf('This is an extension') !== -1);

    // Check the checkboxes visibility and state. They should be visible
    // only if the associated option is enabled, and checked if the
    // associated option is active.
    const accessOptions = [
      {key: 'incognitoAccess', id: '#allow-incognito'},
      {key: 'fileAccess', id: '#allow-on-file-urls'},
      {key: 'errorCollection', id: '#collect-errors'},
    ];
    const isChecked = id => item.$$(id).checked;
    for (let option of accessOptions) {
      expectTrue(isVisible(item, option.id));
      expectFalse(isChecked(option.id), option.id);
      item.set('data.' + option.key + '.isEnabled', false);
      flush();
      expectFalse(isVisible(item, option.id));
      item.set('data.' + option.key + '.isEnabled', true);
      item.set('data.' + option.key + '.isActive', true);
      flush();
      expectTrue(isVisible(item, option.id));
      expectTrue(isChecked(option.id));
    }

    expectFalse(testIsVisible('#dependent-extensions-list'));
    item.set(
        'data.dependentExtensions',
        [{id: 'aaa', name: 'Dependent1'}, {id: 'bbb', name: 'Dependent2'}]);
    flush();
    expectTrue(testIsVisible('#dependent-extensions-list'));
    expectEquals(
        2, item.$$('#dependent-extensions-list').querySelectorAll('li').length);

    expectFalse(testIsVisible('#permissions-list'));
    expectFalse(testIsVisible('#host-access'));
    expectFalse(testIsVisible('extensions-runtime-host-permissions'));

    expectTrue(testIsVisible('#no-permissions'));
    item.set(
        'data.permissions',
        {simplePermissions: ['Permission 1', 'Permission 2']});
    flush();
    expectTrue(testIsVisible('#permissions-list'));
    expectEquals(2, item.$$('#permissions-list').querySelectorAll('li').length);
    expectFalse(testIsVisible('#no-permissions'));
    expectFalse(testIsVisible('#host-access'));
    expectFalse(testIsVisible('extensions-runtime-host-permissions'));

    const optionsUrl =
        'chrome-extension://' + extensionData.id + '/options.html';
    item.set('data.optionsPage', {openInTab: true, url: optionsUrl});
    expectTrue(testIsVisible('#extensions-options'));

    expectFalse(testIsVisible('#extensionsActivityLogLink'));
    item.set('showActivityLog', true);
    flush();
    expectTrue(testIsVisible('#extensionsActivityLogLink'));

    item.set('data.manifestHomePageUrl', 'http://example.com');
    flush();
    expectTrue(testIsVisible('#extensionWebsite'));
    item.set('data.manifestHomePageUrl', '');
    flush();
    expectFalse(testIsVisible('#extensionWebsite'));

    item.set('data.webStoreUrl', 'http://example.com');
    flush();
    expectTrue(testIsVisible('#viewInStore'));
    item.set('data.webStoreUrl', '');
    flush();
    expectFalse(testIsVisible('#viewInStore'));

    expectFalse(testIsVisible('#id-section'));
    expectFalse(testIsVisible('#inspectable-views'));

    item.set('inDevMode', true);
    flush();
    expectTrue(testIsVisible('#id-section'));
    expectTrue(testIsVisible('#inspectable-views'));

    assertTrue(item.data.incognitoAccess.isEnabled);
    item.set('incognitoAvailable', false);
    flush();
    expectFalse(testIsVisible('#allow-incognito'));

    item.set('incognitoAvailable', true);
    flush();
    expectTrue(testIsVisible('#allow-incognito'));

    // Ensure that the "Extension options" button is disabled when the item
    // itself is disabled.
    const extensionOptions = item.$$('#extensions-options');
    assertFalse(extensionOptions.disabled);
    item.set('data.state', chrome.developerPrivate.ExtensionState.DISABLED);
    flush();
    assertTrue(extensionOptions.disabled);

    expectFalse(testIsVisible('.warning-icon'));
    item.set('data.runtimeWarnings', ['Dummy warning']);
    flush();
    expectTrue(testIsVisible('.warning-icon'));

    expectTrue(testIsVisible('#enable-toggle'));
    expectFalse(testIsVisible('#terminated-reload-button'));
    item.set('data.state', chrome.developerPrivate.ExtensionState.TERMINATED);
    flush();
    expectFalse(testIsVisible('#enable-toggle'));
    expectTrue(testIsVisible('#terminated-reload-button'));

    // Ensure that the runtime warning reload button is not visible if there
    // are runtime warnings and the extension is terminated.
    item.set('data.runtimeWarnings', ['Dummy warning']);
    flush();
    expectFalse(testIsVisible('#warnings-reload-button'));
    item.set('data.runtimeWarnings', []);

    // Reset item state back to DISABLED.
    item.set('data.state', chrome.developerPrivate.ExtensionState.DISABLED);
    flush();

    // Ensure that without runtimeHostPermissions data, the sections are
    // hidden.
    expectTrue(testIsVisible('#no-site-access'));
    expectFalse(testIsVisible('extensions-runtime-host-permissions'));
    expectFalse(testIsVisible('extensions-host-permissions-toggle-list'));

    // Adding any runtime host permissions should result in the runtime host
    // controls becoming visible.
    const allSitesPermissions = {
      simplePermissions: [],
      runtimeHostPermissions: {
        hosts: [{granted: false, host: '<all_urls>'}],
        hasAllHosts: true,
        hostAccess: chrome.developerPrivate.HostAccess.ON_CLICK,
      },
    };
    item.set('data.permissions', allSitesPermissions);
    flush();
    expectFalse(testIsVisible('#no-site-access'));
    expectTrue(testIsVisible('extensions-runtime-host-permissions'));
    expectFalse(testIsVisible('extensions-host-permissions-toggle-list'));

    const someSitesPermissions = {
      simplePermissions: [],
      runtimeHostPermissions: {
        hosts: [
          {granted: true, host: 'https://chromium.org/*'},
          {granted: false, host: 'https://example.com/*'}
        ],
        hasAllHosts: false,
        hostAccess: chrome.developerPrivate.HostAccess.ON_SPECIFIC_SITES,
      },
    };
    item.set('data.permissions', someSitesPermissions);
    flush();
    expectFalse(testIsVisible('#no-site-access'));
    expectFalse(testIsVisible('extensions-runtime-host-permissions'));
    expectTrue(testIsVisible('extensions-host-permissions-toggle-list'));
  });

  test(assert(extension_detail_view_tests.TestNames.LayoutSource), function() {
    item.set('data.location', 'FROM_STORE');
    flush();
    assertEquals('Chrome Web Store', item.$.source.textContent.trim());
    assertFalse(isVisible(item, '#load-path'));

    item.set('data.location', 'THIRD_PARTY');
    flush();
    assertEquals('Added by a third-party', item.$.source.textContent.trim());
    assertFalse(isVisible(item, '#load-path'));

    item.set('data.location', 'UNPACKED');
    item.set('data.prettifiedPath', 'foo/bar/baz/');
    flush();
    assertEquals('Unpacked extension', item.$.source.textContent.trim());
    // Test whether the load path is displayed for unpacked extensions.
    assertTrue(isVisible(item, '#load-path'));

    item.set('data.location', 'UNKNOWN');
    item.set('data.prettifiedPath', '');
    // |locationText| is expected to always be set if location is UNKNOWN.
    item.set('data.locationText', 'Foo');
    flush();
    assertEquals('Foo', item.$.source.textContent.trim());
    assertFalse(isVisible(item, '#load-path'));
  });

  test(
      assert(extension_detail_view_tests.TestNames.ClickableElements),
      function() {
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
        item.$$('#extensionsActivityLogLink').click();
        expectDeepEquals(
            currentPage,
            {page: Page.ACTIVITY_LOG, extensionId: extensionData.id});

        // Reset current page and test delegate calls.
        navigation.navigateTo(
            {page: Page.DETAILS, extensionId: extensionData.id});
        currentPage = null;

        mockDelegate.testClickingCalls(
            item.$$('#allow-incognito').getLabel(), 'setItemAllowedIncognito',
            [extensionData.id, true]);
        mockDelegate.testClickingCalls(
            item.$$('#allow-on-file-urls').getLabel(),
            'setItemAllowedOnFileUrls', [extensionData.id, true]);
        mockDelegate.testClickingCalls(
            item.$$('#collect-errors').getLabel(), 'setItemCollectsErrors',
            [extensionData.id, true]);
        mockDelegate.testClickingCalls(
            item.$$('#extensions-options'), 'showItemOptionsPage',
            [extensionData]);
        mockDelegate.testClickingCalls(
            item.$$('#remove-extension'), 'deleteItem', [extensionData.id]);
        mockDelegate.testClickingCalls(
            item.$$('#load-path > a[is=\'action-link\']'), 'showInFolder',
            [extensionData.id]);
        mockDelegate.testClickingCalls(
            item.$$('#warnings-reload-button'), 'reloadItem',
            [extensionData.id], Promise.resolve());

        // Terminate the extension so the reload button appears.
        item.set(
            'data.state', chrome.developerPrivate.ExtensionState.TERMINATED);
        flush();
        mockDelegate.testClickingCalls(
            item.$$('#terminated-reload-button'), 'reloadItem',
            [extensionData.id], Promise.resolve());
      });

  test(assert(extension_detail_view_tests.TestNames.Indicator), function() {
    const indicator = item.$$('cr-tooltip-icon');
    expectTrue(indicator.hidden);
    item.set('data.controlledInfo', {type: 'POLICY', text: 'policy'});
    flush();
    expectFalse(indicator.hidden);
  });

  test(assert(extension_detail_view_tests.TestNames.Warnings), function() {
    const testWarningVisible = function(id, expectVisible) {
      const f = expectVisible ? expectTrue : expectFalse;
      f(isVisible(item, id));
    };

    testWarningVisible('#runtime-warnings', false);
    testWarningVisible('#corrupted-warning', false);
    testWarningVisible('#suspicious-warning', false);
    testWarningVisible('#blacklisted-warning', false);
    testWarningVisible('#update-required-warning', false);

    item.set('data.runtimeWarnings', ['Dummy warning']);
    flush();
    testWarningVisible('#runtime-warnings', true);
    testWarningVisible('#corrupted-warning', false);
    testWarningVisible('#suspicious-warning', false);
    testWarningVisible('#blacklisted-warning', false);
    testWarningVisible('#update-required-warning', false);

    item.set('data.disableReasons.corruptInstall', true);
    flush();
    testWarningVisible('#runtime-warnings', true);
    testWarningVisible('#corrupted-warning', true);
    testWarningVisible('#suspicious-warning', false);
    testWarningVisible('#blacklisted-warning', false);
    testWarningVisible('#update-required-warning', false);

    item.set('data.disableReasons.suspiciousInstall', true);
    flush();
    testWarningVisible('#runtime-warnings', true);
    testWarningVisible('#corrupted-warning', true);
    testWarningVisible('#suspicious-warning', true);
    testWarningVisible('#blacklisted-warning', false);
    testWarningVisible('#update-required-warning', false);

    item.set('data.blacklistText', 'This item is blacklisted');
    flush();
    testWarningVisible('#runtime-warnings', true);
    testWarningVisible('#corrupted-warning', true);
    testWarningVisible('#suspicious-warning', true);
    testWarningVisible('#blacklisted-warning', true);
    testWarningVisible('#update-required-warning', false);

    item.set('data.blacklistText', null);
    flush();
    testWarningVisible('#runtime-warnings', true);
    testWarningVisible('#corrupted-warning', true);
    testWarningVisible('#suspicious-warning', true);
    testWarningVisible('#blacklisted-warning', false);
    testWarningVisible('#update-required-warning', false);

    item.set('data.disableReasons.updateRequired', true);
    flush();
    testWarningVisible('#runtime-warnings', true);
    testWarningVisible('#corrupted-warning', true);
    testWarningVisible('#suspicious-warning', true);
    testWarningVisible('#blacklisted-warning', false);
    testWarningVisible('#update-required-warning', true);

    item.set('data.runtimeWarnings', []);
    item.set('data.disableReasons.corruptInstall', false);
    item.set('data.disableReasons.suspiciousInstall', false);
    item.set('data.disableReasons.updateRequired', false);
    flush();
    testWarningVisible('#runtime-warnings', false);
    testWarningVisible('#corrupted-warning', false);
    testWarningVisible('#suspicious-warning', false);
    testWarningVisible('#blacklisted-warning', false);
    testWarningVisible('#update-required-warning', false);
  });
});
