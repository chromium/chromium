// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extensions-detail-view. */
cr.define('extension_detail_view_tests', function() {
  /** @enum {string} */
  const TestNames = {
    Layout: 'layout',
    LayoutSource: 'layout of source section',
    ClickableElements: 'clickable elements',
    Indicator: 'indicator',
    Warnings: 'warnings',
  };

  const suiteName = 'ExtensionDetailViewTest';

  suite(suiteName, function() {
    /**
     * Extension item created before each test.
     * @type {extensions.Item}
     */
    let item;

    /**
     * Backing extension data for the item.
     * @type {chrome.developerPrivate.ExtensionInfo}
     */
    let extensionData;

    /** @type {extension_test_util.MockItemDelegate} */
    let mockDelegate;

    // Initialize an extension item before each test.
    setup(function() {
      PolymerTest.clearBody();
      extensionData = extension_test_util.createExtensionInfo({
        incognitoAccess: {isEnabled: true, isActive: false},
        fileAccess: {isEnabled: true, isActive: false},
        errorCollection: {isEnabled: true, isActive: false},
      });
      mockDelegate = new extension_test_util.MockItemDelegate();
      item = new extensions.DetailView();
      item.set('data', extensionData);
      item.set('delegate', mockDelegate);
      item.set('inDevMode', false);
      item.set('incognitoAvailable', true);
      item.set('showActivityLog', false);
      document.body.appendChild(item);
    });

    test(assert(TestNames.Layout), function() {
      Polymer.dom.flush();

      extension_test_util.testIcons(item);

      const testIsVisible = extension_test_util.isVisible.bind(null, item);
      expectTrue(testIsVisible('#closeButton'));
      expectTrue(testIsVisible('#icon'));
      expectTrue(testIsVisible('#enable-toggle'));
      expectFalse(testIsVisible('#extensions-options'));
      expectTrue(
          item.$.description.textContent.indexOf('This is an extension') !==
          -1);

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
        expectTrue(extension_test_util.isVisible(item, option.id));
        expectFalse(isChecked(option.id), option.id);
        item.set('data.' + option.key + '.isEnabled', false);
        Polymer.dom.flush();
        expectFalse(extension_test_util.isVisible(item, option.id));
        item.set('data.' + option.key + '.isEnabled', true);
        item.set('data.' + option.key + '.isActive', true);
        Polymer.dom.flush();
        expectTrue(extension_test_util.isVisible(item, option.id));
        expectTrue(isChecked(option.id));
      }

      expectFalse(testIsVisible('#dependent-extensions-list'));
      item.set(
          'data.dependentExtensions',
          [{id: 'aaa', name: 'Dependent1'}, {id: 'bbb', name: 'Dependent2'}]);
      Polymer.dom.flush();
      expectTrue(testIsVisible('#dependent-extensions-list'));
      expectEquals(
          2,
          item.$$('#dependent-extensions-list').querySelectorAll('li').length);

      expectFalse(testIsVisible('#permissions-list'));
      expectFalse(testIsVisible('#host-access'));
      expectFalse(testIsVisible('extensions-runtime-host-permissions'));

      expectTrue(testIsVisible('#no-permissions'));
      item.set(
          'data.permissions',
          {simplePermissions: ['Permission 1', 'Permission 2']});
      Polymer.dom.flush();
      expectTrue(testIsVisible('#permissions-list'));
      expectEquals(
          2, item.$$('#permissions-list').querySelectorAll('li').length);
      expectFalse(testIsVisible('#no-permissions'));
      expectFalse(testIsVisible('#host-access'));
      expectFalse(testIsVisible('extensions-runtime-host-permissions'));

      const optionsUrl =
          'chrome-extension://' + extensionData.id + '/options.html';
      item.set('data.optionsPage', {openInTab: true, url: optionsUrl});
      expectTrue(testIsVisible('#extensions-options'));

      expectFalse(testIsVisible('#extensions-activity-log-link'));
      item.set('showActivityLog', true);
      Polymer.dom.flush();
      expectTrue(testIsVisible('#extensions-activity-log-link'));

      item.set('data.manifestHomePageUrl', 'http://example.com');
      Polymer.dom.flush();
      expectTrue(testIsVisible('#extensionWebsite'));
      item.set('data.manifestHomePageUrl', '');
      Polymer.dom.flush();
      expectFalse(testIsVisible('#extensionWebsite'));

      item.set('data.webStoreUrl', 'http://example.com');
      Polymer.dom.flush();
      expectTrue(testIsVisible('#viewInStore'));
      item.set('data.webStoreUrl', '');
      Polymer.dom.flush();
      expectFalse(testIsVisible('#viewInStore'));

      expectFalse(testIsVisible('#id-section'));
      expectFalse(testIsVisible('#inspectable-views'));

      item.set('inDevMode', true);
      Polymer.dom.flush();
      expectTrue(testIsVisible('#id-section'));
      expectTrue(testIsVisible('#inspectable-views'));

      assertTrue(item.data.incognitoAccess.isEnabled);
      item.set('incognitoAvailable', false);
      Polymer.dom.flush();
      expectFalse(testIsVisible('#allow-incognito'));

      item.set('incognitoAvailable', true);
      Polymer.dom.flush();
      expectTrue(testIsVisible('#allow-incognito'));

      // Ensure that the "Extension options" button is disabled when the item
      // itself is disabled.
      const extensionOptions = item.$$('#extensions-options');
      assertFalse(extensionOptions.disabled);
      item.set('data.state', chrome.developerPrivate.ExtensionState.DISABLED);
      Polymer.dom.flush();
      assertTrue(extensionOptions.disabled);

      expectFalse(testIsVisible('.warning-icon'));
      item.set('data.runtimeWarnings', ['Dummy warning']);
      Polymer.dom.flush();
      expectTrue(testIsVisible('.warning-icon'));

      // Adding any runtime host permissions should result in the runtime host
      // controls becoming visible.
      item.set(
          'data.permissions.hostAccess',
          chrome.developerPrivate.HostAccess.ON_CLICK);
      Polymer.dom.flush();
      expectTrue(testIsVisible('extensions-runtime-host-permissions'));
    });

    test(assert(TestNames.LayoutSource), function() {
      item.set('data.location', 'FROM_STORE');
      Polymer.dom.flush();
      assertEquals('Chrome Web Store', item.$.source.textContent.trim());
      assertFalse(extension_test_util.isVisible(item, '#load-path'));

      item.set('data.location', 'THIRD_PARTY');
      Polymer.dom.flush();
      assertEquals('Added by a third-party', item.$.source.textContent.trim());
      assertFalse(extension_test_util.isVisible(item, '#load-path'));

      item.set('data.location', 'UNPACKED');
      item.set('data.prettifiedPath', 'foo/bar/baz/');
      Polymer.dom.flush();
      assertEquals('Unpacked extension', item.$.source.textContent.trim());
      // Test whether the load path is displayed for unpacked extensions.
      assertTrue(extension_test_util.isVisible(item, '#load-path'));

      item.set('data.location', 'UNKNOWN');
      item.set('data.prettifiedPath', '');
      // |locationText| is expected to always be set if location is UNKNOWN.
      item.set('data.locationText', 'Foo');
      Polymer.dom.flush();
      assertEquals('Foo', item.$.source.textContent.trim());
      assertFalse(extension_test_util.isVisible(item, '#load-path'));
    });

    test(assert(TestNames.ClickableElements), function() {
      const optionsUrl =
          'chrome-extension://' + extensionData.id + '/options.html';
      item.set('data.optionsPage', {openInTab: true, url: optionsUrl});
      item.set('data.prettifiedPath', 'foo/bar/baz/');
      item.set('showActivityLog', true);
      Polymer.dom.flush();

      let currentPage = null;
      extensions.navigation.addListener(newPage => {
        currentPage = newPage;
      });

      // Even though the command line flag is not set for activity log, we
      // still expect to navigate to it after clicking the link as the logic to
      // redirect the page back to the details view is in manager.js.
      // Since this behavior does not happen in the testing environment,
      // we test the behavior in manager_test.js.
      MockInteractions.tap(item.$$('#extensions-activity-log-link'));
      expectDeepEquals(
          currentPage,
          {page: Page.ACTIVITY_LOG, extensionId: extensionData.id});

      // Reset current page and test delegate calls.
      extensions.navigation.navigateTo(
          {page: Page.DETAILS, extensionId: extensionData.id});
      currentPage = null;

      mockDelegate.testClickingCalls(
          item.$$('#allow-incognito').getLabel(), 'setItemAllowedIncognito',
          [extensionData.id, true]);
      mockDelegate.testClickingCalls(
          item.$$('#allow-on-file-urls').getLabel(), 'setItemAllowedOnFileUrls',
          [extensionData.id, true]);
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
          item.$$('#reload-button'), 'reloadItem', [extensionData.id],
          Promise.resolve());
    });

    test(assert(TestNames.Indicator), function() {
      const indicator = item.$$('cr-tooltip-icon');
      expectTrue(indicator.hidden);
      item.set('data.controlledInfo', {type: 'POLICY', text: 'policy'});
      Polymer.dom.flush();
      expectFalse(indicator.hidden);
    });

    test(assert(TestNames.Warnings), function() {
      const testWarningVisible = function(id, isVisible) {
        const f = isVisible ? expectTrue : expectFalse;
        f(extension_test_util.isVisible(item, id));
      };

      testWarningVisible('#runtime-warnings', false);
      testWarningVisible('#corrupted-warning', false);
      testWarningVisible('#suspicious-warning', false);
      testWarningVisible('#blacklisted-warning', false);
      testWarningVisible('#update-required-warning', false);

      item.set('data.runtimeWarnings', ['Dummy warning']);
      Polymer.dom.flush();
      testWarningVisible('#runtime-warnings', true);
      testWarningVisible('#corrupted-warning', false);
      testWarningVisible('#suspicious-warning', false);
      testWarningVisible('#blacklisted-warning', false);
      testWarningVisible('#update-required-warning', false);

      item.set('data.disableReasons.corruptInstall', true);
      Polymer.dom.flush();
      testWarningVisible('#runtime-warnings', true);
      testWarningVisible('#corrupted-warning', true);
      testWarningVisible('#suspicious-warning', false);
      testWarningVisible('#blacklisted-warning', false);
      testWarningVisible('#update-required-warning', false);

      item.set('data.disableReasons.suspiciousInstall', true);
      Polymer.dom.flush();
      testWarningVisible('#runtime-warnings', true);
      testWarningVisible('#corrupted-warning', true);
      testWarningVisible('#suspicious-warning', true);
      testWarningVisible('#blacklisted-warning', false);
      testWarningVisible('#update-required-warning', false);

      item.set('data.blacklistText', 'This item is blacklisted');
      Polymer.dom.flush();
      testWarningVisible('#runtime-warnings', true);
      testWarningVisible('#corrupted-warning', true);
      testWarningVisible('#suspicious-warning', true);
      testWarningVisible('#blacklisted-warning', true);
      testWarningVisible('#update-required-warning', false);

      item.set('data.blacklistText', null);
      Polymer.dom.flush();
      testWarningVisible('#runtime-warnings', true);
      testWarningVisible('#corrupted-warning', true);
      testWarningVisible('#suspicious-warning', true);
      testWarningVisible('#blacklisted-warning', false);
      testWarningVisible('#update-required-warning', false);

      item.set('data.disableReasons.updateRequired', true);
      Polymer.dom.flush();
      testWarningVisible('#runtime-warnings', true);
      testWarningVisible('#corrupted-warning', true);
      testWarningVisible('#suspicious-warning', true);
      testWarningVisible('#blacklisted-warning', false);
      testWarningVisible('#update-required-warning', true);

      item.set('data.runtimeWarnings', []);
      item.set('data.disableReasons.corruptInstall', false);
      item.set('data.disableReasons.suspiciousInstall', false);
      item.set('data.disableReasons.updateRequired', false);
      Polymer.dom.flush();
      testWarningVisible('#runtime-warnings', false);
      testWarningVisible('#corrupted-warning', false);
      testWarningVisible('#suspicious-warning', false);
      testWarningVisible('#blacklisted-warning', false);
      testWarningVisible('#update-required-warning', false);
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
