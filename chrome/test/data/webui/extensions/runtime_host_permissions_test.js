// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://extensions/extensions.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {eventToPromise, isVisible} from '../test_util.m.js';

import {TestService} from './test_service.js';

suite('RuntimeHostPermissions', function() {
  /** @type {RuntimeHostPermissionsElement} */ let element;
  /** @type {TestService} */ let delegate;

  const HostAccess = chrome.developerPrivate.HostAccess;
  const ITEM_ID = 'a'.repeat(32);

  setup(function() {
    PolymerTest.clearBody();
    element = document.createElement('extensions-runtime-host-permissions');
    delegate = new TestService();
    element.delegate = delegate;
    element.itemId = ITEM_ID;

    document.body.appendChild(element);
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

    const testIsVisible = isVisible.bind(null, element);
    expectTrue(testIsVisible('#host-access'));

    const selectHostAccess = element.$$('#host-access');
    expectEquals(HostAccess.ON_CLICK, selectHostAccess.selected);
    // For on-click mode, there should be no runtime hosts listed.
    expectFalse(testIsVisible('#hosts'));

    // Changing the data's access should change the UI appropriately.
    element.set('permissions.hostAccess', HostAccess.ON_ALL_SITES);
    flush();
    expectEquals(HostAccess.ON_ALL_SITES, selectHostAccess.selected);
    expectFalse(testIsVisible('#hosts'));

    // Setting the mode to on specific sites should display the runtime hosts
    // list.
    element.set('permissions.hostAccess', HostAccess.ON_SPECIFIC_SITES);
    element.set('permissions.hosts', [
      {host: 'https://example.com', granted: true},
      {host: 'https://chromium.org', granted: true}
    ]);
    flush();
    expectEquals(HostAccess.ON_SPECIFIC_SITES, selectHostAccess.selected);
    expectTrue(testIsVisible('#hosts'));
    // Expect three entries in the list: the two hosts + the add-host button.
    expectEquals(3, element.$$('#hosts').getElementsByTagName('li').length);
    expectTrue(testIsVisible('#add-host'));
  });

  test('permissions selection', function() {
    const permissions = {
      hostAccess: HostAccess.ON_CLICK,
      hasAllHosts: true,
      hosts: [{granted: false, host: 'https://*.com/*'}],
    };

    element.set('permissions', permissions);
    flush();

    const selectHostAccess = element.$$('#host-access');
    assertTrue(!!selectHostAccess);

    // Changes the value of the selectHostAccess menu and fires the change
    // event, then verifies that the delegate was called with the correct
    // value.
    function expectDelegateCallOnAccessChange(newValue) {
      selectHostAccess.selected = newValue;
      return delegate.whenCalled('setItemHostAccess').then((args) => {
        expectEquals(ITEM_ID, args[0] /* id */);
        expectEquals(newValue, args[1] /* access */);
        delegate.resetResolver('setItemHostAccess');
      });
    }

    // Check that selecting different values correctly notifies the delegate.
    return expectDelegateCallOnAccessChange(HostAccess.ON_ALL_SITES)
        .then(() => {
          return expectDelegateCallOnAccessChange(HostAccess.ON_CLICK);
        });
  });

  test('on select sites cancel', function() {
    const permissions = {
      hostAccess: HostAccess.ON_CLICK,
      hasAllHosts: true,
      hosts: [{granted: false, host: 'https://*/*'}],
    };

    element.permissions = permissions;
    flush();

    const selectHostAccess = element.$$('#host-access');
    assertTrue(!!selectHostAccess);

    selectHostAccess.selected = HostAccess.ON_SPECIFIC_SITES;

    flush();
    const dialog = element.$$('extensions-runtime-hosts-dialog');
    assertTrue(!!dialog);

    expectTrue(dialog.updateHostAccess);

    // Canceling the dialog should reset the selectHostAccess value to ON_CLICK,
    // since no host was added.
    assertTrue(dialog.isOpen());
    let whenClosed = eventToPromise('close', dialog);
    dialog.$$('.cancel-button').click();
    return whenClosed.then(() => {
      flush();
      expectEquals(HostAccess.ON_CLICK, selectHostAccess.selected);
    });
  });

  test('on select sites accept', function() {
    const permissions = {
      hostAccess: HostAccess.ON_CLICK,
      hasAllHosts: true,
      hosts: [{granted: false, host: 'https://*/*'}],
    };

    element.set('permissions', permissions);
    flush();

    const selectHostAccess = element.$$('#host-access');
    assertTrue(!!selectHostAccess);

    selectHostAccess.selected = HostAccess.ON_SPECIFIC_SITES;

    flush();
    const dialog = element.$$('extensions-runtime-hosts-dialog');
    assertTrue(!!dialog);

    expectTrue(dialog.updateHostAccess);

    // Make the add button clickable by entering valid input.
    const input = dialog.$$('cr-input');
    input.value = 'https://example.com';
    input.fire('input');

    // Closing the dialog (as opposed to canceling) should keep the
    // selectHostAccess value at ON_SPECIFIC_SITES.
    assertTrue(dialog.isOpen());
    let whenClosed = eventToPromise('close', dialog);
    dialog.$$('.action-button').click();
    return whenClosed.then(() => {
      flush();
      expectEquals(HostAccess.ON_SPECIFIC_SITES, selectHostAccess.selected);

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
      const editHost = element.$$('.edit-host');
      editHost.click();
      const actionMenu = element.$$('cr-action-menu');
      const actionMenuEdit = actionMenu.querySelector('#action-menu-edit');
      assertTrue(!!actionMenuEdit);
      actionMenuEdit.click();
      flush();

      // Verify that the dialog does not want to update the old host access.
      // Regression test for https://crbug.com/903082.
      const dialog = element.$$('extensions-runtime-hosts-dialog');
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

    const addHostButton = element.$$('#add-host');
    assertTrue(!!addHostButton);
    expectTrue(isVisible(element, '#add-host'));

    addHostButton.click();
    flush();
    const dialog = element.$$('extensions-runtime-hosts-dialog');
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

    const editHost = element.$$('.edit-host');
    assertTrue(!!editHost);
    editHost.click();
    const actionMenu = element.$$('cr-action-menu');
    assertTrue(!!actionMenu);
    expectTrue(actionMenu.open);

    const remove = actionMenu.querySelector('#action-menu-remove');
    assertTrue(!!remove);

    remove.click();
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

    const editHost = element.$$('.edit-host');
    editHost.click();
    const actionMenu = element.$$('cr-action-menu');

    const actionMenuEdit = actionMenu.querySelector('#action-menu-edit');
    assertTrue(!!actionMenuEdit);

    actionMenuEdit.click();
    flush();
    const dialog = element.$$('extensions-runtime-hosts-dialog');
    assertTrue(!!dialog);
    expectTrue(dialog.$.dialog.open);
    expectFalse(dialog.updateHostAccess);
    expectEquals('https://chromium.org', dialog.currentSite);
  });
});
