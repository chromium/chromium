// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://extensions/extensions.js';

import type {ExtensionsRuntimeHostPermissionsElement} from 'chrome://extensions/extensions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isChildVisible} from 'chrome://webui-test/test_util.js';

import {TestService} from './test_service.js';
import {MetricsPrivateMock} from './test_util.js';

suite('RuntimeHostPermissions', function() {
  let element: ExtensionsRuntimeHostPermissionsElement;
  let delegate: TestService;
  let metricsPrivateMock: MetricsPrivateMock;

  const HostAccess = chrome.developerPrivate.HostAccess;
  const ITEM_ID = 'a'.repeat(32);

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('extensions-runtime-host-permissions');
    delegate = new TestService();
    delegate.userSiteSettings = {permittedSites: [], restrictedSites: []};
    element.delegate = delegate;
    element.itemId = ITEM_ID;
    element.enableEnhancedSiteControls = false;

    document.body.appendChild(element);

    metricsPrivateMock = new MetricsPrivateMock();
    chrome.metricsPrivate =
        metricsPrivateMock as unknown as typeof chrome.metricsPrivate;
  });

  teardown(function() {
    element.remove();
  });

  test('permissions display', function() {
    const permissions: chrome.developerPrivate.RuntimeHostPermissions = {
      hostAccess: HostAccess.ON_CLICK,
      hasAllHosts: true,
      hosts: [{granted: false, host: 'https://*/*'}],
    };

    element.permissions = permissions;
    flush();

    const testIsVisible = isChildVisible.bind(null, element);
    assertTrue(testIsVisible('#hostAccess'));

    const selectHostAccess = element.getSelectMenu();
    assertEquals(HostAccess.ON_CLICK, selectHostAccess.value);
    // For on-click mode, there should be no runtime hosts listed.
    assertFalse(testIsVisible('#hosts'));

    // Changing the data's access should change the UI appropriately.
    element.set('permissions.hostAccess', HostAccess.ON_ALL_SITES);
    flush();
    assertEquals(HostAccess.ON_ALL_SITES, selectHostAccess.value);
    assertFalse(testIsVisible('#hosts'));

    // Setting the mode to on specific sites should display the runtime hosts
    // list.
    element.set('permissions.hostAccess', HostAccess.ON_SPECIFIC_SITES);
    element.set('permissions.hosts', [
      {host: 'https://example.com', granted: true},
      {host: 'https://chromium.org', granted: true},
    ]);
    flush();
    assertEquals(HostAccess.ON_SPECIFIC_SITES, selectHostAccess.value);
    assertTrue(testIsVisible('#hosts'));
    // Expect three entries in the list: the two hosts + the add-host button.
    assertEquals(
        3,
        element.shadowRoot!.querySelector('#hosts')!.getElementsByTagName('li')
            .length);
    assertTrue(testIsVisible('#add-host'));
  });

  test('permissions display with enableEnhancedSiteControls flag', function() {
    element.enableEnhancedSiteControls = true;
    const permissions: chrome.developerPrivate.RuntimeHostPermissions = {
      hostAccess: HostAccess.ON_CLICK,
      hasAllHosts: true,
      hosts: [
        {host: 'https://example.com', granted: true},
        {host: 'https://chromium.org', granted: true},
      ],
    };

    element.permissions = permissions;
    flush();

    const testIsVisible = isChildVisible.bind(null, element);
    assertTrue(testIsVisible('#newHostAccess'));
    assertTrue(testIsVisible('#new-section-heading'));

    const selectHostAccess = element.getSelectMenu();
    assertEquals(HostAccess.ON_CLICK, selectHostAccess.value);
    // For on-click mode, there should be no runtime hosts listed.
    assertFalse(testIsVisible('#hosts'));
    assertFalse(testIsVisible('#add-site-button'));

    // Changing the data's access should change the UI appropriately.
    element.set('permissions.hostAccess', HostAccess.ON_ALL_SITES);
    flush();
    assertEquals(HostAccess.ON_ALL_SITES, selectHostAccess.value);
    assertFalse(testIsVisible('#hosts'));
    assertFalse(testIsVisible('#add-site-button'));

    element.set('permissions.hostAccess', HostAccess.ON_SPECIFIC_SITES);
    flush();
    assertEquals(HostAccess.ON_SPECIFIC_SITES, selectHostAccess.value);
    assertTrue(testIsVisible('#hosts'));
    assertTrue(testIsVisible('#add-site-button'));
    assertFalse(testIsVisible('#add-host'));
  });

  test('permissions selection', async () => {
    const permissions: chrome.developerPrivate.RuntimeHostPermissions = {
      hostAccess: HostAccess.ON_CLICK,
      hasAllHosts: true,
      hosts: [{granted: false, host: 'https://*.com/*'}],
    };

    element.permissions = permissions;
    flush();

    const selectHostAccess = element.getSelectMenu();
    assertTrue(!!selectHostAccess);

    // Changes the value of the selectHostAccess menu and fires the change
    // event, then verifies that the delegate was called with the correct
    // value.
    async function assertDelegateCallOnAccessChange(
        newValue: chrome.developerPrivate.HostAccess): Promise<void> {
      selectHostAccess.value = newValue;
      selectHostAccess.dispatchEvent(new CustomEvent('change'));
      const args = await delegate.whenCalled('setItemHostAccess');
      assertEquals(ITEM_ID, args[0] /* id */);
      assertEquals(newValue, args[1] /* access */);
      delegate.resetResolver('setItemHostAccess');
    }

    // Check that selecting different values correctly notifies the delegate.
    await assertDelegateCallOnAccessChange(HostAccess.ON_ALL_SITES);
    assertEquals(
        metricsPrivateMock.getUserActionCount(
            'Extensions.Settings.Hosts.OnAllSitesSelected'),
        1);
    await assertDelegateCallOnAccessChange(HostAccess.ON_CLICK);
    assertEquals(
        metricsPrivateMock.getUserActionCount(
            'Extensions.Settings.Hosts.OnClickSelected'),
        1);
    // Finally select back to all sites and ensure the user action for it has
    // incremented.
    await assertDelegateCallOnAccessChange(HostAccess.ON_ALL_SITES);
    assertEquals(
        metricsPrivateMock.getUserActionCount(
            'Extensions.Settings.Hosts.OnAllSitesSelected'),
        2);
  });

  test('on select sites cancel', async () => {
    const permissions: chrome.developerPrivate.RuntimeHostPermissions = {
      hostAccess: HostAccess.ON_CLICK,
      hasAllHosts: true,
      hosts: [{granted: false, host: 'https://*/*'}],
    };

    element.permissions = permissions;
    flush();

    const selectHostAccess = element.getSelectMenu();
    assertTrue(!!selectHostAccess);

    selectHostAccess.value = HostAccess.ON_SPECIFIC_SITES;
    selectHostAccess.dispatchEvent(new CustomEvent('change'));

    flush();
    const dialog =
        element.shadowRoot!.querySelector('extensions-runtime-hosts-dialog');
    assertTrue(!!dialog);
    assertEquals(
        metricsPrivateMock.getUserActionCount(
            'Extensions.Settings.Hosts.OnClickSelected'),
        0);
    assertEquals(
        metricsPrivateMock.getUserActionCount(
            'Extensions.Settings.Hosts.OnSpecificSitesSelected'),
        1);

    assertTrue(dialog.updateHostAccess);

    // Canceling the dialog should reset the selectHostAccess value to ON_CLICK,
    // since no host was added.
    assertTrue(dialog.isOpen());
    const whenClosed = eventToPromise('close', dialog);
    dialog.shadowRoot!.querySelector<HTMLElement>('.cancel-button')!.click();
    await whenClosed;

    flush();
    assertEquals(HostAccess.ON_CLICK, selectHostAccess.value);
    assertEquals(
        metricsPrivateMock.getUserActionCount(
            'Extensions.Settings.Hosts.AddHostDialogCanceled'),
        1);
    // Reverting to a previous option when canceling the dialog doesn't count
    // as a user action, so the on-click action count should still be 0.
    assertEquals(
        metricsPrivateMock.getUserActionCount(
            'Extensions.Settings.Hosts.OnClickSelected'),
        0);
    // Changing to a different option after this should still log a user action
    // as asserted.
    selectHostAccess.value = HostAccess.ON_ALL_SITES;
    selectHostAccess.dispatchEvent(new CustomEvent('change'));
    flush();
    assertEquals(
        metricsPrivateMock.getUserActionCount(
            'Extensions.Settings.Hosts.OnAllSitesSelected'),
        1);
  });

  test('on select sites accept', async () => {
    const permissions: chrome.developerPrivate.RuntimeHostPermissions = {
      hostAccess: HostAccess.ON_CLICK,
      hasAllHosts: true,
      hosts: [{granted: false, host: 'https://*/*'}],
    };

    element.permissions = permissions;
    flush();

    const selectHostAccess = element.getSelectMenu();
    assertTrue(!!selectHostAccess);

    selectHostAccess.value = HostAccess.ON_SPECIFIC_SITES;
    selectHostAccess.dispatchEvent(new CustomEvent('change'));
    assertEquals(
        metricsPrivateMock.getUserActionCount(
            'Extensions.Settings.Hosts.OnSpecificSitesSelected'),
        1);

    flush();
    const dialog =
        element.shadowRoot!.querySelector('extensions-runtime-hosts-dialog');
    assertTrue(!!dialog);

    assertTrue(dialog.updateHostAccess);

    // Make the add button clickable by entering valid input.
    const input = dialog.shadowRoot!.querySelector('cr-input');
    assertTrue(!!input);
    input.value = 'https://example.com';
    input.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));
    await input.updateComplete;

    // Closing the dialog (as opposed to canceling) should keep the
    // selectHostAccess value at ON_SPECIFIC_SITES.
    assertTrue(dialog.isOpen());
    const whenClosed = eventToPromise('close', dialog);
    dialog.$.submit.click();
    await whenClosed;
    flush();
    assertEquals(HostAccess.ON_SPECIFIC_SITES, selectHostAccess.value);
    assertEquals(
        metricsPrivateMock.getUserActionCount(
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
    const editHost =
        element.shadowRoot!.querySelector<HTMLElement>('.open-edit-host');
    assertTrue(!!editHost);
    editHost.click();
    assertEquals(
        metricsPrivateMock.getUserActionCount(
            'Extensions.Settings.Hosts.ActionMenuOpened'),
        1);
    const actionMenu = element.shadowRoot!.querySelector('cr-action-menu');
    assertTrue(!!actionMenu);
    const actionMenuEdit =
        actionMenu.querySelector<HTMLElement>('#action-menu-edit');
    assertTrue(!!actionMenuEdit);
    actionMenuEdit.click();
    flush();
    assertEquals(
        metricsPrivateMock.getUserActionCount(
            'Extensions.Settings.Hosts.ActionMenuEditActivated'),
        1);

    // Verify that the dialog does not want to update the old host access.
    // Regression test for https://crbug.com/903082.
    const newDialog =
        element.shadowRoot!.querySelector('extensions-runtime-hosts-dialog');
    assertTrue(!!newDialog);
    assertTrue(newDialog.$.dialog.open);
    assertFalse(newDialog.updateHostAccess);
    assertEquals('https://example.com/*', newDialog.currentSite);
  });

  test('clicking add host triggers dialog', function() {
    const permissions: chrome.developerPrivate.RuntimeHostPermissions = {
      hostAccess: HostAccess.ON_SPECIFIC_SITES,
      hasAllHosts: true,
      hosts: [
        {host: 'https://www.example.com/*', granted: true},
        {host: 'https://*.google.com', granted: false},
        {host: '*://*.com/*', granted: false},
      ],
    };

    element.permissions = permissions;
    flush();

    const addHostButton =
        element.shadowRoot!.querySelector<HTMLElement>('#add-host');
    assertTrue(!!addHostButton);
    assertTrue(isChildVisible(element, '#add-host'));

    addHostButton.click();
    flush();
    assertEquals(
        metricsPrivateMock.getUserActionCount(
            'Extensions.Settings.Hosts.AddHostActivated'),
        1);
    const dialog =
        element.shadowRoot!.querySelector('extensions-runtime-hosts-dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.$.dialog.open);
    assertEquals(null, dialog.currentSite);
    assertFalse(dialog.updateHostAccess);
  });

  test('removing runtime host permissions', async function() {
    const permissions: chrome.developerPrivate.RuntimeHostPermissions = {
      hostAccess: HostAccess.ON_SPECIFIC_SITES,
      hasAllHosts: true,
      hosts: [
        {host: 'https://example.com', granted: true},
        {host: 'https://chromium.org', granted: true},
        {host: '*://*.com/*', granted: false},
      ],
    };
    element.permissions = permissions;
    flush();

    const editHost =
        element.shadowRoot!.querySelector<HTMLElement>('.open-edit-host');
    assertTrue(!!editHost);
    editHost.click();
    assertEquals(
        metricsPrivateMock.getUserActionCount(
            'Extensions.Settings.Hosts.ActionMenuOpened'),
        1);
    const actionMenu = element.shadowRoot!.querySelector('cr-action-menu');
    assertTrue(!!actionMenu);
    assertTrue(actionMenu.open);

    const remove = actionMenu.querySelector<HTMLElement>('#action-menu-remove');
    assertTrue(!!remove);

    remove.click();
    assertEquals(
        metricsPrivateMock.getUserActionCount(
            'Extensions.Settings.Hosts.ActionMenuRemoveActivated'),
        1);
    const [id, site] = await delegate.whenCalled('removeRuntimeHostPermission');
    assertEquals(ITEM_ID, id);
    assertEquals('https://chromium.org', site);
    assertFalse(actionMenu.open);
  });

  test('clicking edit host triggers dialog', function() {
    const permissions: chrome.developerPrivate.RuntimeHostPermissions = {
      hostAccess: HostAccess.ON_SPECIFIC_SITES,
      hasAllHosts: true,
      hosts: [
        {host: 'https://example.com', granted: true},
        {host: 'https://chromium.org', granted: true},
        {host: '*://*.com/*', granted: false},
      ],
    };
    element.permissions = permissions;
    flush();

    const editHost =
        element.shadowRoot!.querySelector<HTMLElement>('.open-edit-host');
    assertTrue(!!editHost);
    editHost.click();
    const actionMenu = element.shadowRoot!.querySelector('cr-action-menu');
    assertTrue(!!actionMenu);

    const actionMenuEdit =
        actionMenu.querySelector<HTMLElement>('#action-menu-edit');
    assertTrue(!!actionMenuEdit);

    actionMenuEdit.click();
    flush();
    const dialog =
        element.shadowRoot!.querySelector('extensions-runtime-hosts-dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.$.dialog.open);
    assertFalse(dialog.updateHostAccess);
    assertEquals('https://chromium.org', dialog.currentSite);
  });

  test('clicking edit host with enableEnhancedSiteControls flag', function() {
    element.enableEnhancedSiteControls = true;
    const permissions: chrome.developerPrivate.RuntimeHostPermissions = {
      hostAccess: HostAccess.ON_SPECIFIC_SITES,
      hasAllHosts: true,
      hosts: [
        {host: 'https://chromium.org', granted: true},
      ],
    };

    element.permissions = permissions;
    flush();

    const editHost =
        element.shadowRoot!.querySelector<HTMLElement>('.edit-host');
    assertTrue(!!editHost);
    editHost.click();
    flush();

    // clicking the `editHost` for the site should open the dialog.
    const dialog =
        element.shadowRoot!.querySelector('extensions-runtime-hosts-dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.$.dialog.open);
    assertFalse(dialog.updateHostAccess);
    assertEquals('https://chromium.org', dialog.currentSite);
  });

  test(
      'clicking remove host with enableEnhancedSiteControls flag',
      async function() {
        element.enableEnhancedSiteControls = true;
        const permissions: chrome.developerPrivate.RuntimeHostPermissions = {
          hostAccess: HostAccess.ON_SPECIFIC_SITES,
          hasAllHosts: true,
          hosts: [
            {host: 'https://chromium.org', granted: true},
          ],
        };

        element.permissions = permissions;
        flush();

        const removeHost =
            element.shadowRoot!.querySelector<HTMLElement>('.remove-host');
        assertTrue(!!removeHost);
        removeHost.click();
        flush();

        const [id, site] =
            await delegate.whenCalled('removeRuntimeHostPermission');
        assertEquals(ITEM_ID, id);
        assertEquals('https://chromium.org', site);
      });

  test(
      'switching away from ON_SPECIFIC_SITES with flag enabled triggers dialog',
      async function() {
        element.enableEnhancedSiteControls = true;

        const permissions: chrome.developerPrivate.RuntimeHostPermissions = {
          hostAccess: HostAccess.ON_SPECIFIC_SITES,
          hasAllHosts: true,
          hosts: [
            {host: 'https://example.com', granted: true},
            {host: 'https://chromium.org', granted: true},
            {host: '*://*.com/*', granted: false},
          ],
        };

        element.permissions = permissions;
        flush();

        const selectHostAccess = element.getSelectMenu();
        assertTrue(!!selectHostAccess);

        // Change the `selectHostAccess` value and the dialog should be open.
        selectHostAccess.value = HostAccess.ON_CLICK;
        selectHostAccess.dispatchEvent(new CustomEvent('change'));
        flush();

        let dialog = element.getRemoveSiteDialog();
        assertTrue(!!dialog);
        assertTrue(dialog.open);

        // Clicking cancel on the dialog should revert the `selectHostAccess`
        // value back to ON_SPECIFIC_SITES.
        const cancel = dialog.querySelector<HTMLElement>('.cancel-button');
        assertTrue(!!cancel);
        cancel.click();

        flush();
        assertFalse(!!element.getRemoveSiteDialog());
        assertEquals(HostAccess.ON_SPECIFIC_SITES, selectHostAccess.value);

        // Change the `selectHostAccess` value and the dialog should be open.
        selectHostAccess.value = HostAccess.ON_CLICK;
        selectHostAccess.dispatchEvent(new CustomEvent('change'));
        flush();

        dialog = element.getRemoveSiteDialog();
        assertTrue(!!dialog);
        assertTrue(dialog.open);

        // Clicking remove on the dialog should a call to the delegate to set
        // the host access to the `selectHostAccess` value.
        const remove = dialog.querySelector<HTMLElement>('.action-button');
        assertTrue(!!remove);
        remove.click();

        const [id, access] = await delegate.whenCalled('setItemHostAccess');
        assertEquals(ITEM_ID, id);
        assertEquals(HostAccess.ON_CLICK, access);

        flush();
        assertFalse(!!element.getRemoveSiteDialog());
        assertEquals(HostAccess.ON_CLICK, selectHostAccess.value);
      });
});
