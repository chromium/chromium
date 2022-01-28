// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ExtensionsHostPermissionsToggleListElement, ExtensionsToggleRowElement, UserAction} from 'chrome://extensions/extensions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestService} from './test_service.js';

suite('HostPermissionsToggleList', function() {
  let element: ExtensionsHostPermissionsToggleListElement;
  let delegate: TestService;

  const HostAccess = chrome.developerPrivate.HostAccess;
  const ITEM_ID = 'a'.repeat(32);
  const EXAMPLE_COM = 'https://example.com/*';
  const GOOGLE_COM = 'https://google.com/*';
  const CHROMIUM_ORG = 'https://chromium.org/*';

  setup(function() {
    document.body.innerHTML = '';
    element = document.createElement('extensions-host-permissions-toggle-list');
    delegate = new TestService();
    element.delegate = delegate;
    element.itemId = ITEM_ID;

    document.body.appendChild(element);
  });

  teardown(function() {
    element.remove();
  });

  // Tests the display of the list when only specific sites are granted.
  test('permissions display for specific sites', function() {
    const permissions = {
      hostAccess: HostAccess.ON_SPECIFIC_SITES,
      hasAllHosts: false,
      hosts: [
        {host: EXAMPLE_COM, granted: true},
        {host: GOOGLE_COM, granted: false},
        {host: CHROMIUM_ORG, granted: true},
      ],
    };

    element.permissions = permissions;
    flush();

    assertTrue(!!element);
    const allSites = element.$.allHostsToggle;
    assertFalse(allSites.checked);

    const hostToggles =
        element.shadowRoot!.querySelectorAll<ExtensionsToggleRowElement>(
            '.host-toggle');
    assertEquals(3, hostToggles.length);

    // There should be three toggles, all enabled, and checked corresponding to
    // whether the host is granted.
    assertEquals(CHROMIUM_ORG, hostToggles[0]!.innerText!.trim());
    assertFalse(hostToggles[0]!.disabled);
    assertTrue(hostToggles[0]!.checked);

    assertEquals(EXAMPLE_COM, hostToggles[1]!.innerText!.trim());
    assertFalse(hostToggles[1]!.disabled);
    assertTrue(hostToggles[1]!.checked);

    assertEquals(GOOGLE_COM, hostToggles[2]!.innerText!.trim());
    assertFalse(hostToggles[2]!.disabled);
    assertFalse(hostToggles[2]!.checked);
  });

  // Tests the display when the user has chosen to allow on all the requested
  // sites.
  test('permissions display for all requested sites', function() {
    const permissions = {
      hostAccess: HostAccess.ON_ALL_SITES,
      hasAllHosts: false,
      hosts: [
        {host: EXAMPLE_COM, granted: true},
        {host: GOOGLE_COM, granted: true},
        {host: CHROMIUM_ORG, granted: true},
      ],
    };

    element.permissions = permissions;
    flush();

    assertTrue(!!element);
    const allSites = element.$.allHostsToggle;
    assertTrue(allSites.checked);

    const hostToggles =
        element.shadowRoot!.querySelectorAll<ExtensionsToggleRowElement>(
            '.host-toggle');
    assertEquals(3, hostToggles.length);

    // There should be three toggles, and they should all be disabled and
    // checked, since the user selected to allow the extension to run on all
    // (requested) sites.
    assertEquals(CHROMIUM_ORG, hostToggles[0]!.innerText!.trim());
    assertTrue(hostToggles[0]!.disabled);
    assertTrue(hostToggles[0]!.checked);

    assertEquals(EXAMPLE_COM, hostToggles[1]!.innerText!.trim());
    assertTrue(hostToggles[1]!.disabled);
    assertTrue(hostToggles[1]!.checked);

    assertEquals(GOOGLE_COM, hostToggles[2]!.innerText!.trim());
    assertTrue(hostToggles[2]!.disabled);
    assertTrue(hostToggles[2]!.checked);
  });

  // Tests the permissions display when a user has chosen to only run an
  // extension on-click.
  test('permissions display for on click', function() {
    const permissions = {
      hostAccess: HostAccess.ON_CLICK,
      hasAllHosts: false,
      hosts: [
        {host: EXAMPLE_COM, granted: false},
        {host: GOOGLE_COM, granted: false},
        {host: CHROMIUM_ORG, granted: false},
      ],
    };

    element.permissions = permissions;
    flush();

    assertTrue(!!element);
    const allSites = element.$.allHostsToggle;
    assertFalse(allSites!.checked);

    const hostToggles =
        element.shadowRoot!.querySelectorAll<ExtensionsToggleRowElement>(
            '.host-toggle');
    assertEquals(3, hostToggles.length);

    // There should be three toggles, all enabled, and all unchecked, since no
    // host has been granted.
    assertEquals(CHROMIUM_ORG, hostToggles[0]!.innerText!.trim());
    assertFalse(hostToggles[0]!.disabled);
    assertFalse(hostToggles[0]!.checked);

    assertEquals(EXAMPLE_COM, hostToggles[1]!.innerText!.trim());
    assertFalse(hostToggles[1]!.disabled);
    assertFalse(hostToggles[1]!.checked);

    assertEquals(GOOGLE_COM, hostToggles[2]!.innerText!.trim());
    assertFalse(hostToggles[2]!.disabled);
    assertFalse(hostToggles[2]!.checked);
  });

  // Tests that clicking the "learn more" button is logged as a user action
  // correctly.
  test('clicking learn more link', async function() {
    const permissions = {
      hostAccess: HostAccess.ON_CLICK,
      hasAllHosts: false,
      hosts: [
        {host: EXAMPLE_COM, granted: false},
      ],
    };

    element.permissions = permissions;
    flush();

    const learnMoreButton = element.$.linkIconButton;
    assertTrue(!!learnMoreButton);
    // Prevent triggering the navigation, which could interfere with tests.
    learnMoreButton.href = '#';
    learnMoreButton.target = '_self';
    learnMoreButton.click();

    const metricName = await delegate.whenCalled('recordUserAction');
    assertEquals(UserAction.LEARN_MORE, metricName);
  });

  // Tests that clicking the "allow on the following sites" toggle when it is in
  // the "off" state calls the delegate as expected.
  test('clicking all hosts toggle from off to on', function() {
    const permissions = {
      hostAccess: HostAccess.ON_CLICK,
      hasAllHosts: false,
      hosts: [
        {host: EXAMPLE_COM, granted: false},
        {host: GOOGLE_COM, granted: false},
        {host: CHROMIUM_ORG, granted: false},
      ],
    };
    element.permissions = permissions;
    flush();

    assertTrue(!!element);
    const allSites = element.$.allHostsToggle;
    allSites.getLabel().click();
    return delegate.whenCalled('setItemHostAccess')
        .then(([id, access]) => {
          assertEquals(ITEM_ID, id);
          assertEquals(HostAccess.ON_ALL_SITES, access);
          return delegate.whenCalled('recordUserAction');
        })
        .then(metricName => {
          assertEquals(UserAction.ALL_TOGGLED_ON, metricName);
        });
  });

  // Tests that clicking the "allow on the following sites" toggle when it is in
  // the "on" state calls the delegate as expected.
  test('clicking all hosts toggle from on to off', function() {
    const permissions = {
      hostAccess: HostAccess.ON_ALL_SITES,
      hasAllHosts: false,
      hosts: [
        {host: EXAMPLE_COM, granted: true},
        {host: GOOGLE_COM, granted: true},
        {host: CHROMIUM_ORG, granted: true},
      ],
    };
    element.permissions = permissions;
    flush();

    assertTrue(!!element);
    const allSites = element.$.allHostsToggle;
    allSites.getLabel().click();
    return delegate.whenCalled('setItemHostAccess')
        .then(([id, access]) => {
          assertEquals(ITEM_ID, id);
          assertEquals(HostAccess.ON_SPECIFIC_SITES, access);
          return delegate.whenCalled('recordUserAction');
        })
        .then((metricName: UserAction) => {
          assertEquals(UserAction.ALL_TOGGLED_OFF, metricName);
        });
  });

  // Tests that toggling a site's enabled state toggles the extension's access
  // to that site properly.
  test('clicking to toggle a specific site', function() {
    const permissions = {
      hostAccess: HostAccess.ON_SPECIFIC_SITES,
      hasAllHosts: false,
      hosts: [
        {host: EXAMPLE_COM, granted: true},
        {host: GOOGLE_COM, granted: false},
        {host: CHROMIUM_ORG, granted: true},
      ],
    };

    element.permissions = permissions;
    flush();

    const hostToggles =
        element.shadowRoot!.querySelectorAll<ExtensionsToggleRowElement>(
            '.host-toggle');
    assertEquals(3, hostToggles.length);

    assertEquals(CHROMIUM_ORG, hostToggles[0]!.innerText!.trim());
    assertEquals(GOOGLE_COM, hostToggles[2]!.innerText!.trim());

    hostToggles[0]!.getLabel().click();
    return delegate.whenCalled('removeRuntimeHostPermission')
        .then(([id, site]) => {
          assertEquals(ITEM_ID, id);
          assertEquals(CHROMIUM_ORG, site);
          return delegate.whenCalled('recordUserAction');
        })
        .then((metricName: UserAction) => {
          assertEquals(UserAction.SPECIFIC_TOGGLED_OFF, metricName);
          delegate.resetResolver('recordUserAction');

          hostToggles[2]!.getLabel().click();
          return delegate.whenCalled('addRuntimeHostPermission');
        })
        .then(([id, site]) => {
          assertEquals(ITEM_ID, id);
          assertEquals(GOOGLE_COM, site);
          return delegate.whenCalled('recordUserAction');
        })
        .then((metricName: UserAction) => {
          assertEquals(UserAction.SPECIFIC_TOGGLED_ON, metricName);
        });
  });
});
