// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://extensions/extensions.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {eventToPromise, isChildVisible} from '../test_util.js';

import {TestService} from './test_service.js';
import {MetricsPrivateMock} from './test_util.js';

suite('RuntimeHostPermissions', function() {
  /** @type {RuntimeHostPermissionsElement} */ let element;
  /** @type {TestService} */ let delegate;
  /** @type {Function} */ let getUserActionCount;

  const HostAccess = chrome.developerPrivate.HostAccess;
  const ITEM_ID = 'a'.repeat(32);

  setup(function() {
    loadTimeData.overrideValues({extensionsMenuAccessControlEnabled: false});
    document.body.innerHTML = '';
    element = document.createElement('extensions-runtime-host-permissions');
    delegate = new TestService();
    element.delegate = delegate;
    element.itemId = ITEM_ID;

    document.body.appendChild(element);

    chrome.metricsPrivate = new MetricsPrivateMock();
    // For convenience, we define a shorter name to call getUserActionCount
    // with.
    getUserActionCount = (...args) =>
        chrome.metricsPrivate.getUserActionCount(...args);
  });

  teardown(function() {
    element.remove();
  });

  test('permissions display', function() {
    const permissions = {
      hostAccess: HostAccess.ON_CLICK,
      hasAllHosts: true,
      hosts: [{granted: false, host: 'https://*/*'}],
    };

    element.set('permissions', permissions);
    flush();

    const testIsVisible = isChildVisible.bind(null, element);
    expectTrue(testIsVisible('#host-access'));

    const selectHostAccess = element.shadowRoot.querySelector('#host-access');
    expectEquals(HostAccess.ON_CLICK, selectHostAccess.value);
    // For on-click mode, there should be no runtime hosts listed.
    expectFalse(testIsVisible('#hosts'));

    // Changing the data's access should change the UI appropriately.
    element.set('permissions.hostAccess', HostAccess.ON_ALL_SITES);
    flush();
    expectEquals(HostAccess.ON_ALL_SITES, selectHostAccess.value);
    expectFalse(testIsVisible('#hosts'));

    // Setting the mode to on specific sites should display the runtime hosts
    // list.
    element.set('permissions.hostAccess', HostAccess.ON_SPECIFIC_SITES);
    element.set('permissions.hosts', [
      {host: 'https://example.com', granted: true},
      {host: 'https://chromium.org', granted: true}
    ]);
    flush();
    expectEquals(HostAccess.ON_SPECIFIC_SITES, selectHostAccess.value);
    expectTrue(testIsVisible('#hosts'));
    // Expect three entries in the list: the two hosts + the add-host button.
    expectEquals(
        3,
        element.shadowRoot.querySelector('#hosts')
            .getElementsByTagName('li')
            .length);
    expectTrue(testIsVisible('#add-host'));
  });

  test('permissions display new site access menu', function() {
    loadTimeData.overrideValues({extensionsMenuAccessControlEnabled: true});
    const permissions = {
      hostAccess: HostAccess.ON_CLICK,
      hasAllHosts: true,
      hosts: [{granted: false, host: 'https://*/*'}],
    };

    element.set('permissions', permissions);
    flush();

    const testIsVisible = isChildVisible.bind(null, element);
    expectTrue(testIsVisible('#host-access'));

    const selectHostAccess = element.shadowRoot.querySelector('#host-access');
    expectEquals(HostAccess.ON_CLICK, selectHostAccess.value);
    // For on-click mode, there should be no runtime hosts listed.
    expectFalse(testIsVisible('#hosts'));

    // Changing the data's access should change the UI appropriately.
    element.set('permissions.hostAccess', HostAccess.ON_ALL_SITES);
    flush();
    expectEquals(HostAccess.ON_ALL_SITES, selectHostAccess.value);
    expectFalse(testIsVisible('#hosts'));

    element.set('permissions.hostAccess', HostAccess.ON_SPECIFIC_SITES);
    flush();
    expectEquals(HostAccess.ON_SPECIFIC_SITES, selectHostAccess.value);
    // TODO(crbug.com/1253673): Test the new "customize for each site" menu.
  });

  test('permissions selection', async () => {
    const permissions = {
      hostAccess: HostAccess.ON_CLICK,
      hasAllHosts: true,
      hosts: [{granted: false, host: 'https://*.com/*'}],
    };

    element.set('permissions', permissions);
    flush();

    const selectHostAccess = element.shadowRoot.querySelector('#host-access');
    assertTrue(!!selectHostAccess);

    // Changes the value of the selectHostAccess menu and fires the change
    // event, then verifies that the delegate was called with the correct
    // value.
    function expectDelegateCallOnAccessChange(newValue) {
      selectHostAccess.value = newValue;
      selectHostAccess.dispatchEvent(new CustomEvent('change'));
      return delegate.whenCalled('setItemHostAccess').then((args) => {
        expectEquals(ITEM_ID, args[0] /* id */);
        expectEquals(newValue, args[1] /* access */);
        delegate.resetResolver('setItemHostAccess');
      });
    }

    // Check that selecting different values correctly notifies the delegate.
    await expectDelegateCallOnAccessChange(HostAccess.ON_ALL_SITES);
    expectEquals(
        getUserActionCount('Extensions.Settings.Hosts.OnAllSitesSelected'), 1);
    await expectDelegateCallOnAccessChange(HostAccess.ON_CLICK);
    expectEquals(
        getUserActionCount('Extensions.Settings.Hosts.OnClickSelected'), 1);
    // Finally select back to all sites and ensure the user action for it has
    // incremented.
    await expectDelegateCallOnAccessChange(HostAccess.ON_ALL_SITES);
    expectEquals(
        getUserActionCount('Extensions.Settings.Hosts.OnAllSitesSelected'), 2);
  });

  test('on select sites cancel', async () => {
    const permissions = {
      hostAccess: HostAccess.ON_CLICK,
      hasAllHosts: true,
      hosts: [{granted: false, host: 'https://*/*'}],
    };

    element.permissions = permissions;
    flush();

    const selectHostAccess = element.shadowRoot.querySelector('#host-access');
    assertTrue(!!selectHostAccess);

    selectHostAccess.value = HostAccess.ON_SPECIFIC_SITES;
    selectHostAccess.dispatchEvent(new CustomEvent('change'));

    flush();
    const dialog =
        element.shadowRoot.querySelector('extensions-runtime-hosts-dialog');
    assertTrue(!!dialog);
    expectEquals(
        getUserActionCount('Extensions.Settings.Hosts.OnClickSelected'), 0);
    expectEquals(
        getUserActionCount('Extensions.Settings.Hosts.OnSpecificSitesSelected'),
        1);

    expectTrue(dialog.updateHostAccess);

    // Canceling the dialog should reset the selectHostAccess value to ON_CLICK,
    // since no host was added.
    assertTrue(dialog.isOpen());
    let whenClosed = eventToPromise('close', dialog);
    dialog.shadowRoot.querySelector('.cancel-button').click();
    await whenClosed;

    flush();
    expectEquals(HostAccess.ON_CLICK, selectHostAccess.value);
    expectEquals(
        getUserActionCount('Extensions.Settings.Hosts.AddHostDialogCanceled'),
        1);
    // Reverting to a previous option when canceling the dialog doesn't count
    // as a user action, so the on-click action count should still be 0.
    expectEquals(
        getUserActionCount('Extensions.Settings.Hosts.OnClickSelected'), 0);
    // Changing to a different option after this should still log a user action
    // as expected.
    selectHostAccess.value = HostAccess.ON_ALL_SITES;
    selectHostAccess.dispatchEvent(new CustomEvent('change'));
    flush();
    expectEquals(
        getUserActionCount('Extensions.Settings.Hosts.OnAllSitesSelected'), 1);
  });

  test('on select sites accept', function() {
    const permissions = {
      hostAccess: HostAccess.ON_CLICK,
      hasAllHosts: true,
      hosts: [{granted: false, host: 'https://*/*'}],
    };

    element.set('permissions', permissions);
    flush();

    const selectHostAccess = element.shadowRoot.querySelector('#host-access');
    assertTrue(!!selectHostAccess);

    selectHostAccess.value = HostAccess.ON_SPECIFIC_SITES;
    selectHostAccess.dispatchEvent(new CustomEvent('change'));
    expectEquals(
        getUserActionCount('Extensions.Settings.Hosts.OnSpecificSitesSelected'),
        1);

    flush();
    const dialog =
        element.shadowRoot.querySelector('extensions-runtime-hosts-dialog');
    assertTrue(!!dialog);

    expectTrue(dialog.updateHostAccess);

    // Make the add button clickable by entering valid input.
    const input = dialog.shadowRoot.querySelector('cr-input');
    input.value = 'https://example.com';
    input.fire('input');

    // Closing the dialog (as opposed to canceling) should keep the
    // selectHostAccess value at ON_SPECIFIC_SITES.
    assertTrue(dialog.isOpen());
    let whenClosed = eventToPromise('close', dialog);
    dialog.shadowRoot.querySelector('.action-button').click();
    return whenClosed.then(() => {
      flush();
      expectEquals(HostAccess.ON_SPECIFIC_SITES, selectHostAccess.value);
      expectEquals(
          getUserActionCount(
              'Extensions.Settings.Hosts.AddHostDialogSubmitted'),
          1);

      // Simulate the new host being added.
      const updatedPermissions = {
        hostAccess: HostAccess.ON_SPECIFIC_SITES,
        hasAllHosts: true,
        hosts: [
          {host: 'https://example.com/*', granted: true},
          {host: 'https://*/*', granted: false},
        ],
      };
      element.permissions = updatedPermissions;
      flush();

      // Open the dialog by clicking to edit the host permission.
      const editHost = element.shadowRoot.querySelector('.edit-host');
      editHost.click();
      expectEquals(
          getUserActionCount('Extensions.Settings.Hosts.ActionMenuOpened'), 1);
      const actionMenu = element.shadowRoot.querySelector('cr-action-menu');
      const actionMenuEdit = actionMenu.querySelector('#action-menu-edit');
      assertTrue(!!actionMenuEdit);
      actionMenuEdit.click();
      flush();
      expectEquals(
          getUserActionCount(
              'Extensions.Settings.Hosts.ActionMenuEditActivated'),
          1);

      // Verify that the dialog does not want to update the old host access.
      // Regression test for https://crbug.com/903082.
      const dialog =
          element.shadowRoot.querySelector('extensions-runtime-hosts-dialog');
      assertTrue(!!dialog);
      expectTrue(dialog.$.dialog.open);
      expectFalse(dialog.updateHostAccess);
      expectEquals('https://example.com/*', dialog.currentSite);
    });
  });

  test('clicking add host triggers dialog', function() {
    const permissions = {
      hostAccess: HostAccess.ON_SPECIFIC_SITES,
      hasAllHosts: true,
      hosts: [
        {host: 'https://www.example.com/*', granted: true},
        {host: 'https://*.google.com', granted: false},
        {host: '*://*.com/*', granted: false},
      ],
    };

    element.set('permissions', permissions);
    flush();

    const addHostButton = element.shadowRoot.querySelector('#add-host');
    assertTrue(!!addHostButton);
    expectTrue(isChildVisible(element, '#add-host'));

    addHostButton.click();
    flush();
    expectEquals(
        getUserActionCount('Extensions.Settings.Hosts.AddHostActivated'), 1);
    const dialog =
        element.shadowRoot.querySelector('extensions-runtime-hosts-dialog');
    assertTrue(!!dialog);
    expectTrue(dialog.$.dialog.open);
    expectEquals(null, dialog.currentSite);
    expectFalse(dialog.updateHostAccess);
  });

  test('removing runtime host permissions', function() {
    const permissions = {
      hostAccess: HostAccess.ON_SPECIFIC_SITES,
      hasAllHosts: true,
      hosts: [
        {host: 'https://example.com', granted: true},
        {host: 'https://chromium.org', granted: true},
        {host: '*://*.com/*', granted: false},
      ],
    };
    element.set('permissions', permissions);
    flush();

    const editHost = element.shadowRoot.querySelector('.edit-host');
    assertTrue(!!editHost);
    editHost.click();
    expectEquals(
        getUserActionCount('Extensions.Settings.Hosts.ActionMenuOpened'), 1);
    const actionMenu = element.shadowRoot.querySelector('cr-action-menu');
    assertTrue(!!actionMenu);
    expectTrue(actionMenu.open);

    const remove = actionMenu.querySelector('#action-menu-remove');
    assertTrue(!!remove);

    remove.click();
    expectEquals(
        getUserActionCount(
            'Extensions.Settings.Hosts.ActionMenuRemoveActivated'),
        1);
    return delegate.whenCalled('removeRuntimeHostPermission').then((args) => {
      expectEquals(ITEM_ID, args[0] /* id */);
      expectEquals('https://chromium.org', args[1] /* site */);
      expectFalse(actionMenu.open);
    });
  });

  test('clicking edit host triggers dialog', function() {
    const permissions = {
      hostAccess: HostAccess.ON_SPECIFIC_SITES,
      hasAllHosts: true,
      hosts: [
        {host: 'https://example.com', granted: true},
        {host: 'https://chromium.org', granted: true},
        {host: '*://*.com/*', granted: false},
      ],
    };
    element.set('permissions', permissions);
    flush();

    const editHost = element.shadowRoot.querySelector('.edit-host');
    editHost.click();
    const actionMenu = element.shadowRoot.querySelector('cr-action-menu');

    const actionMenuEdit = actionMenu.querySelector('#action-menu-edit');
    assertTrue(!!actionMenuEdit);

    actionMenuEdit.click();
    flush();
    const dialog =
        element.shadowRoot.querySelector('extensions-runtime-hosts-dialog');
    assertTrue(!!dialog);
    expectTrue(dialog.$.dialog.open);
    expectFalse(dialog.updateHostAccess);
    expectEquals('https://chromium.org', dialog.currentSite);
  });
});
