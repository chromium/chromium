// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://extensions/extensions.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {TestService} from './test_service.js';

suite('HostPermissionsToggleList', function() {
  /** @type {HostPermissionsToggleListElement} */ let element;
  /** @type {TestService} */ let delegate;

  const HostAccess = chrome.developerPrivate.HostAccess;
  const ITEM_ID = 'a'.repeat(32);
  const EXAMPLE_COM = 'https://example.com/*';
  const GOOGLE_COM = 'https://google.com/*';
  const CHROMIUM_ORG = 'https://chromium.org/*';

  setup(function() {
    PolymerTest.clearBody();
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

    assertTrue(!!element.$);
    const allSites = element.$.allHostsToggle;
    expectFalse(allSites.checked);

    const hostToggles = element.shadowRoot.querySelectorAll('.host-toggle');
    assertEquals(3, hostToggles.length);

    // There should be three toggles, all enabled, and checked corresponding to
    // whether the host is granted.
    expectEquals(CHROMIUM_ORG, hostToggles[0].innerText.trim());
    expectFalse(hostToggles[0].disabled);
    expectTrue(hostToggles[0].checked);

    expectEquals(EXAMPLE_COM, hostToggles[1].innerText.trim());
    expectFalse(hostToggles[1].disabled);
    expectTrue(hostToggles[1].checked);

    expectEquals(GOOGLE_COM, hostToggles[2].innerText.trim());
    expectFalse(hostToggles[2].disabled);
    expectFalse(hostToggles[2].checked);
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

    assertTrue(!!element.$);
    const allSites = element.$.allHostsToggle;
    expectTrue(allSites.checked);

    const hostToggles = element.shadowRoot.querySelectorAll('.host-toggle');
    assertEquals(3, hostToggles.length);

    // There should be three toggles, and they should all be disabled and
    // checked, since the user selected to allow the extension to run on all
    // (requested) sites.
    expectEquals(CHROMIUM_ORG, hostToggles[0].innerText.trim());
    expectTrue(hostToggles[0].disabled);
    expectTrue(hostToggles[0].checked);

    expectEquals(EXAMPLE_COM, hostToggles[1].innerText.trim());
    expectTrue(hostToggles[1].disabled);
    expectTrue(hostToggles[1].checked);

    expectEquals(GOOGLE_COM, hostToggles[2].innerText.trim());
    expectTrue(hostToggles[2].disabled);
    expectTrue(hostToggles[2].checked);
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

    assertTrue(!!element.$);
    const allSites = element.$.allHostsToggle;
    expectFalse(allSites.checked);

    const hostToggles = element.shadowRoot.querySelectorAll('.host-toggle');
    assertEquals(3, hostToggles.length);

    // There should be three toggles, all enabled, and all unchecked, since no
    // host has been granted.
    expectEquals(CHROMIUM_ORG, hostToggles[0].innerText.trim());
    expectFalse(hostToggles[0].disabled);
    expectFalse(hostToggles[0].checked);

    expectEquals(EXAMPLE_COM, hostToggles[1].innerText.trim());
    expectFalse(hostToggles[1].disabled);
    expectFalse(hostToggles[1].checked);

    expectEquals(GOOGLE_COM, hostToggles[2].innerText.trim());
    expectFalse(hostToggles[2].disabled);
    expectFalse(hostToggles[2].checked);
  });

  // Tests that clicking the "allow on all sites" toggle changes the item
  // host access properly.
  test('clicking all hosts toggle', function() {
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

    assertTrue(!!element.$);
    const allSites = element.$.allHostsToggle;
    allSites.getLabel().click();
    return delegate.whenCalled('setItemHostAccess').then((args) => {
      expectEquals(ITEM_ID, args[0] /* id */);
      expectEquals(HostAccess.ON_ALL_SITES, args[1] /* access */);
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

    const hostToggles = element.shadowRoot.querySelectorAll('.host-toggle');
    assertEquals(3, hostToggles.length);

    expectEquals(CHROMIUM_ORG, hostToggles[0].innerText.trim());
    expectEquals(GOOGLE_COM, hostToggles[2].innerText.trim());

    hostToggles[0].getLabel().click();
    return delegate.whenCalled('removeRuntimeHostPermission')
        .then((args) => {
          expectEquals(ITEM_ID, args[0] /* id */);
          expectEquals(CHROMIUM_ORG, args[1] /* site */);

          hostToggles[2].getLabel().click();
          return delegate.whenCalled('addRuntimeHostPermission');
        })
        .then((args) => {
          expectEquals(ITEM_ID, args[0] /* id */);
          expectEquals(GOOGLE_COM, args[1] /* site */);
        });
  });
});
