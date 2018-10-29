// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('RuntimeHostPermissions', function() {
  /** @type {extensions.RuntimeHostPermissionsElement} */ let element;
  /** @type {extensions.TestService} */ let delegate;

  const HostAccess = chrome.developerPrivate.HostAccess;
  const ITEM_ID = 'a'.repeat(32);

  setup(function() {
    PolymerTest.clearBody();
    element = document.createElement('extensions-runtime-host-permissions');
    delegate = new extensions.TestService();
    element.delegate = delegate;
    element.itemId = ITEM_ID;

    document.body.appendChild(element);
  });

  teardown(function() {
    element.remove();
  });

  test('permissions display', function() {
    const permissions = {
      simplePermissions: ['permission 1', 'permission 2'],
      hostAccess: HostAccess.ON_CLICK,
    };

    element.set('permissions', permissions);
    Polymer.dom.flush();

    const testIsVisible = extension_test_util.isVisible.bind(null, element);
    expectTrue(testIsVisible('#host-access'));

    const selectHostAccess = element.$$('#host-access');
    expectEquals(HostAccess.ON_CLICK, selectHostAccess.value);
    // For on-click mode, there should be no runtime hosts listed.
    expectFalse(testIsVisible('#hosts'));
    expectFalse(testIsVisible('#add-hosts-section'));

    // Changing the data's access should change the UI appropriately.
    element.set('permissions.hostAccess', HostAccess.ON_ALL_SITES);
    Polymer.dom.flush();
    expectEquals(HostAccess.ON_ALL_SITES, selectHostAccess.value);
    expectFalse(testIsVisible('#hosts'));
    expectFalse(testIsVisible('#add-hosts-section'));

    // Setting the mode to on specific sites should display the runtime hosts
    // list.
    element.set('permissions.hostAccess', HostAccess.ON_SPECIFIC_SITES);
    element.set('permissions.specificSiteControls', {
      hasAllHosts: false,
      hosts: [
        {host: 'https://example.com', granted: true},
        {host: 'https://chromium.org', granted: true}
      ],
    });
    Polymer.dom.flush();
    expectEquals(HostAccess.ON_SPECIFIC_SITES, selectHostAccess.value);
    expectTrue(testIsVisible('#hosts'));
    expectTrue(testIsVisible('#add-hosts-section'));
    expectEquals(2, element.$$('#hosts').getElementsByTagName('li').length);
  });

  test('permissions selection', function() {
    const permissions = {
      simplePermissions: ['permission 1', 'permission 2'],
      hostAccess: HostAccess.ON_CLICK,
    };

    element.set('permissions', permissions);
    Polymer.dom.flush();

    const selectHostAccess = element.$$('#host-access');
    assertTrue(!!selectHostAccess);

    // Changes the value of the selectHostAccess menu and fires the change
    // event, then verifies that the delegate was called with the correct
    // value.
    function expectDelegateCallOnAccessChange(newValue) {
      selectHostAccess.value = newValue;
      selectHostAccess.dispatchEvent(
          new CustomEvent('change', {target: selectHostAccess}));
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
    };

    element.permissions = permissions;
    Polymer.dom.flush();

    const selectHostAccess = element.$$('#host-access');
    assertTrue(!!selectHostAccess);

    selectHostAccess.value = HostAccess.ON_SPECIFIC_SITES;
    selectHostAccess.dispatchEvent(new CustomEvent('change'));

    Polymer.dom.flush();
    const dialog = element.$$('extensions-runtime-hosts-dialog');
    assertTrue(!!dialog);

    expectTrue(dialog.updateHostAccess);

    // Canceling the dialog should reset the selectHostAccess value to ON_CLICK,
    // since no host was added.
    assertTrue(dialog.isOpen());
    let whenClosed = test_util.eventToPromise('close', dialog);
    dialog.$$('.cancel-button').click();
    return whenClosed.then(() => {
      Polymer.dom.flush();
      expectEquals(HostAccess.ON_CLICK, selectHostAccess.value);
    });
  });

  test('on select sites accept', function() {
    const permissions = {
      simplePermissions: ['permission 1', 'permission 2'],
      hostAccess: HostAccess.ON_CLICK,
    };

    element.set('permissions', permissions);
    Polymer.dom.flush();

    const selectHostAccess = element.$$('#host-access');
    assertTrue(!!selectHostAccess);

    selectHostAccess.value = HostAccess.ON_SPECIFIC_SITES;
    selectHostAccess.dispatchEvent(
        new CustomEvent('change', {target: selectHostAccess}));

    Polymer.dom.flush();
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
    let whenClosed = test_util.eventToPromise('close', dialog);
    dialog.$$('.action-button').click();
    return whenClosed.then(() => {
      Polymer.dom.flush();
      expectEquals(HostAccess.ON_SPECIFIC_SITES, selectHostAccess.value);
    });
  });

  test('clicking add host triggers dialog', function() {
    const permissions = {
      simplePermissions: [],
      hostAccess: HostAccess.ON_SPECIFIC_SITES,
      specificSiteControls: {
        hasAllHosts: false,
        hosts: [
          {host: 'https://www.example.com/*', granted: true},
          {host: 'https://*.google.com', granted: false}
        ],
      },
    };

    element.set('permissions', permissions);
    Polymer.dom.flush();

    const addHostButton = element.$$('#add-host');
    assertTrue(!!addHostButton);
    expectTrue(extension_test_util.isVisible(element, '#add-host'));

    addHostButton.click();
    Polymer.dom.flush();
    const dialog = element.$$('extensions-runtime-hosts-dialog');
    assertTrue(!!dialog);
    expectTrue(dialog.$.dialog.open);
    expectEquals(null, dialog.currentSite);
    expectFalse(dialog.updateHostAccess);
  });

  test('removing runtime host permissions', function() {
    const permissions = {
      simplePermissions: [],
      hostAccess: HostAccess.ON_SPECIFIC_SITES,
      specificSiteControls: {
        hasAllHosts: false,
        hosts: [
          {host: 'https://example.com', granted: true},
          {host: 'https://chromium.org', granted: true}
        ],
      },
    };
    element.set('permissions', permissions);
    Polymer.dom.flush();

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
      simplePermissions: [],
      hostAccess: HostAccess.ON_SPECIFIC_SITES,
      specificSiteControls: {
        hasAllHosts: false,
        hosts: [
          {host: 'https://example.com', granted: true},
          {host: 'https://chromium.org', granted: true}
        ],
      },
    };
    element.set('permissions', permissions);
    Polymer.dom.flush();

    const editHost = element.$$('.edit-host');
    editHost.click();
    const actionMenu = element.$$('cr-action-menu');

    const actionMenuEdit = actionMenu.querySelector('#action-menu-edit');
    assertTrue(!!actionMenuEdit);

    actionMenuEdit.click();
    Polymer.dom.flush();
    const dialog = element.$$('extensions-runtime-hosts-dialog');
    assertTrue(!!dialog);
    expectTrue(dialog.$.dialog.open);
    expectFalse(dialog.updateHostAccess);
    expectEquals('https://chromium.org', dialog.currentSite);
  });
});
