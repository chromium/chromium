// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://test/cr_elements/cr_policy_strings.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ContentSettingsTypes, LocalDataBrowserProxyImpl, SiteSettingsPrefsBrowserProxyImpl, SortMethod} from 'chrome://settings/lazy_load.js';
import {Router, routes} from 'chrome://settings/settings.js';
import {TestLocalDataBrowserProxy} from 'chrome://test/settings/test_local_data_browser_proxy.js';
import {TestSiteSettingsPrefsBrowserProxy} from 'chrome://test/settings/test_site_settings_prefs_browser_proxy.js';
import {createSiteGroup} from 'chrome://test/settings/test_util.js';
import {eventToPromise} from 'chrome://test/test_util.m.js';

// clang-format on

suite('SiteEntry', function() {
  /**
   * An example eTLD+1 Object with multiple origins grouped under it.
   * @type {!SiteGroup}
   */
  const TEST_MULTIPLE_SITE_GROUP = createSiteGroup('example.com', [
    'http://example.com',
    'https://www.example.com',
    'https://login.example.com',
  ]);

  /**
   * An example eTLD+1 Object with a single origin in it.
   * @type {!SiteGroup}
   */
  const TEST_SINGLE_SITE_GROUP = createSiteGroup('foo.com', [
    'https://login.foo.com',
  ]);

  const TEST_COOKIE_LIST = {
    id: 'foo',
    children: [
      {domain: 'example.com'},
      {domain: 'example.com'},
      {domain: 'example.com'},
    ]
  };

  /**
   * The mock proxy object to use during test.
   * @type {TestSiteSettingsPrefsBrowserProxy}
   */
  let browserProxy;

  /**
   * The mock local data proxy object to use during test.
   * @type {TestLocalDataBrowserProxy}
   */
  let localDataBrowserProxy;

  /**
   * A site list element created before each test.
   * @type {SiteList}
   */
  let testElement;

  /**
   * The clickable element that expands to show the list of origins.
   * @type {Element}
   */
  let toggleButton;

  // Initialize a site-list before each test.
  setup(function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    localDataBrowserProxy = new TestLocalDataBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
    LocalDataBrowserProxyImpl.instance_ = localDataBrowserProxy;

    PolymerTest.clearBody();
    testElement = document.createElement('site-entry');
    assertTrue(!!testElement);
    document.body.appendChild(testElement);

    toggleButton = testElement.$.toggleButton;
  });

  teardown(function() {
    // The code being tested changes the Route. Reset so that state is not
    // leaked across tests.
    Router.getInstance().resetRouteForTesting();
  });

  test('displays the correct number of origins', function() {
    testElement.siteGroup = TEST_MULTIPLE_SITE_GROUP;
    flush();
    const collapseChild = testElement.$.originList.get();
    flush();
    assertEquals(3, collapseChild.querySelectorAll('.origin-link').length);
  });

  test('expands and closes to show more origins', function() {
    testElement.siteGroup = TEST_MULTIPLE_SITE_GROUP;
    assertTrue(testElement.grouped_(testElement.siteGroup));
    assertEquals('false', toggleButton.getAttribute('aria-expanded'));
    const originList = testElement.$.originList.get();
    assertTrue(originList.classList.contains('iron-collapse-closed'));
    assertEquals('true', originList.getAttribute('aria-hidden'));

    toggleButton.click();
    assertEquals('true', toggleButton.getAttribute('aria-expanded'));
    assertTrue(originList.classList.contains('iron-collapse-opened'));
    assertEquals('false', originList.getAttribute('aria-hidden'));
  });

  test('with single origin navigates to Site Details', function() {
    testElement.siteGroup = TEST_SINGLE_SITE_GROUP;
    assertFalse(testElement.grouped_(testElement.siteGroup));
    assertEquals('false', toggleButton.getAttribute('aria-expanded'));
    const originList = testElement.$.originList.get();
    assertTrue(originList.classList.contains('iron-collapse-closed'));
    assertEquals('true', originList.getAttribute('aria-hidden'));

    toggleButton.click();
    assertEquals('false', toggleButton.getAttribute('aria-expanded'));
    assertTrue(originList.classList.contains('iron-collapse-closed'));
    assertEquals('true', originList.getAttribute('aria-hidden'));
    assertEquals(
        routes.SITE_SETTINGS_SITE_DETAILS.path,
        Router.getInstance().getCurrentRoute().path);
    assertEquals(
        'https://login.foo.com',
        Router.getInstance().getQueryParameters().get('site'));
  });

  test('with multiple origins navigates to Site Details', function() {
    testElement.siteGroup = TEST_MULTIPLE_SITE_GROUP;
    flush();
    const collapseChild = testElement.$.originList.get();
    flush();
    const originList = collapseChild.querySelectorAll('.origin-link');
    assertEquals(3, originList.length);

    // Test clicking on one of these origins takes the user to Site Details,
    // with the correct origin.
    originList[1].click();
    assertEquals(
        routes.SITE_SETTINGS_SITE_DETAILS.path,
        Router.getInstance().getCurrentRoute().path);
    assertEquals(
        TEST_MULTIPLE_SITE_GROUP.origins[1].origin,
        Router.getInstance().getQueryParameters().get('site'));
  });

  test('with single origin, shows overflow menu', function() {
    testElement.siteGroup = TEST_SINGLE_SITE_GROUP;
    flush();
    const overflowMenuButton = testElement.$.overflowMenuButton;
    assertFalse(overflowMenuButton.closest('.row-aligned').hidden);
  });

  test('clear data for single origin fires the right method', async function() {
    testElement.siteGroup =
        JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
    flush();

    const collapseChild = testElement.$.originList.get();
    flush();
    const originList = collapseChild.querySelectorAll('.hr');
    assertEquals(3, originList.length);

    for (let i = 0; i < originList.length; i++) {
      const menuOpened = eventToPromise('open-menu', testElement);
      const originEntry = originList[i];
      const overflowMenuButton =
          originEntry.querySelector('#originOverflowMenuButton');
      overflowMenuButton.click();
      const openMenuEvent = await menuOpened;

      const args = openMenuEvent.detail;
      const {actionScope, index, origin} = args;
      assertEquals('origin', actionScope);
      assertEquals(testElement.listIndex, index);
      assertEquals(testElement.siteGroup.origins[i].origin, origin);
    }
  });

  test(
      'moving from grouped to ungrouped does not get stuck in opened state',
      function() {
        // Clone this object to avoid propagating changes made in this test.
        testElement.siteGroup =
            JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
        flush();
        toggleButton.click();
        assertTrue(testElement.$.originList.get().opened);

        // Remove all origins except one, then make sure it's not still
        // expanded.
        testElement.siteGroup.origins.splice(1);
        assertEquals(1, testElement.siteGroup.origins.length);
        testElement.onSiteGroupChanged_(testElement.siteGroup);
        assertFalse(testElement.$.originList.get().opened);
      });

  test('cookies show correctly for grouped entries', function() {
    localDataBrowserProxy.setCookieDetails(TEST_COOKIE_LIST);
    testElement.siteGroup = TEST_MULTIPLE_SITE_GROUP;
    flush();
    const cookiesLabel = testElement.$.cookies;
    assertTrue(cookiesLabel.hidden);
    // When the number of cookies is more than zero, the label appears.
    const testSiteGroup = JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
    const numCookies = 3;
    testSiteGroup.numCookies = numCookies;

    testElement.siteGroup = testSiteGroup;

    flush();
    return localDataBrowserProxy.whenCalled('getNumCookiesString')
        .then((args) => {
          assertEquals(3, args);
          assertFalse(cookiesLabel.hidden);
          assertEquals('· 3 cookies', cookiesLabel.textContent.trim());
        });
  });

  test('cookies show for ungrouped entries', function() {
    testElement.siteGroup = TEST_SINGLE_SITE_GROUP;
    flush();
    const cookiesLabel = testElement.$.cookies;
    assertTrue(cookiesLabel.hidden);


    const testSiteGroup = JSON.parse(JSON.stringify(TEST_SINGLE_SITE_GROUP));
    const numCookies = 3;

    testSiteGroup.numCookies = numCookies;

    testElement.siteGroup = testSiteGroup;

    flush();
    return localDataBrowserProxy.whenCalled('getNumCookiesString')
        .then((args) => {
          assertEquals(3, args);
          assertFalse(cookiesLabel.hidden);
          assertEquals('· 3 cookies', cookiesLabel.textContent.trim());
        });
  });

  test('data usage shown correctly for grouped entries', function() {
    // Clone this object to avoid propagating changes made in this test.
    const testSiteGroup = JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
    const numBytes1 = 74622;
    const numBytes2 = 1274;
    const numBytes3 = 0;
    testSiteGroup.origins[0].usage = numBytes1;
    testSiteGroup.origins[1].usage = numBytes2;
    testSiteGroup.origins[2].usage = numBytes3;
    testElement.siteGroup = testSiteGroup;
    flush();
    return browserProxy.whenCalled('getFormattedBytes').then((args) => {
      const sumBytes = numBytes1 + numBytes2 + numBytes3;
      assertEquals(
          `${sumBytes} B`,
          testElement.root.querySelector('#displayName .data-unit')
              .textContent.trim());
    });
  });

  test('data usage shown correctly for ungrouped entries', function() {
    // Clone this object to avoid propagating changes made in this test.
    const testSiteGroup = JSON.parse(JSON.stringify(TEST_SINGLE_SITE_GROUP));
    const numBytes = 74622;
    testSiteGroup.origins[0].usage = numBytes;
    testElement.siteGroup = testSiteGroup;
    flush();
    return browserProxy.whenCalled('getFormattedBytes').then((args) => {
      assertEquals(
          `${numBytes} B`,
          testElement.root.querySelector('#displayName .data-unit')
              .textContent.trim());
    });
  });

  test(
      'large number data usage shown correctly for grouped entries',
      function() {
        // Clone this object to avoid propagating changes made in this test.
        const testSiteGroup =
            JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
        const numBytes1 = 2000000000;
        const numBytes2 = 10000000000;
        const numBytes3 = 7856;
        testSiteGroup.origins[0].usage = numBytes1;
        testSiteGroup.origins[1].usage = numBytes2;
        testSiteGroup.origins[2].usage = numBytes3;
        testElement.siteGroup = testSiteGroup;
        flush();
        return browserProxy.whenCalled('getFormattedBytes').then((args) => {
          const sumBytes = numBytes1 + numBytes2 + numBytes3;
          assertEquals(
              `${sumBytes} B`,
              testElement.root.querySelector('#displayName .data-unit')
                  .textContent.trim());
        });
      });

  test('favicon with www.etld+1 chosen for site group', function() {
    // Clone this object to avoid propagating changes made in this test.
    const testSiteGroup = JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
    testSiteGroup.origins[0].usage = 0;
    testSiteGroup.origins[1].usage = 1274;
    testSiteGroup.origins[2].usage = 74622;
    testElement.siteGroup = testSiteGroup;
    flush();
    assertEquals(
        testElement.$.collapseParent.querySelector('site-favicon').url,
        'https://www.example.com');
  });

  test('favicon with largest storage chosen for site group', function() {
    // Clone this object to avoid propagating changes made in this test.
    const testSiteGroup = JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
    testSiteGroup.origins[0].usage = 0;
    testSiteGroup.origins[1].usage = 1274;
    testSiteGroup.origins[2].usage = 74622;
    testSiteGroup.origins[1].origin = 'https://abc.example.com';
    testElement.siteGroup = testSiteGroup;
    flush();
    assertEquals(
        testElement.$.collapseParent.querySelector('site-favicon').url,
        'https://login.example.com');
  });

  test('favicon with largest cookies number chosen for site group', function() {
    // Clone this object to avoid propagating changes made in this test.
    const testSiteGroup = JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
    testSiteGroup.origins[0].usage = 0;
    testSiteGroup.origins[1].usage = 1274;
    testSiteGroup.origins[2].usage = 1274;
    testSiteGroup.origins[0].numCookies = 10;
    testSiteGroup.origins[1].numCookies = 3;
    testSiteGroup.origins[2].numCookies = 1;
    testSiteGroup.origins[1].origin = 'https://abc.example.com';
    testElement.siteGroup = testSiteGroup;
    flush();
    assertEquals(
        testElement.$.collapseParent.querySelector('site-favicon').url,
        'https://abc.example.com');
  });

  test('can be sorted by most visited', function() {
    // Clone this object to avoid propagating changes made in this test.
    const testSiteGroup = JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
    testSiteGroup.origins[0].engagement = 20;
    testSiteGroup.origins[1].engagement = 30;
    testSiteGroup.origins[2].engagement = 10;
    testSiteGroup.origins[0].usage = 0;
    testSiteGroup.origins[1].usage = 1274;
    testSiteGroup.origins[2].usage = 1274;
    testSiteGroup.origins[0].numCookies = 10;
    testSiteGroup.origins[1].numCookies = 3;
    testSiteGroup.origins[2].numCookies = 1;
    testElement.sortMethod = SortMethod.MOST_VISITED;
    testElement.siteGroup = testSiteGroup;
    flush();
    const collapseChild = testElement.$.originList.get();
    flush();
    const origins = collapseChild.querySelectorAll('.origin-link');
    assertEquals(3, origins.length);
    assertEquals(
        'www.example.com',
        origins[0].querySelector('#originSiteRepresentation').innerText.trim());
    assertEquals(
        'example.com',
        origins[1].querySelector('#originSiteRepresentation').innerText.trim());
    assertEquals(
        'login.example.com',
        origins[2].querySelector('#originSiteRepresentation').innerText.trim());
  });

  test('can be sorted by storage', function() {
    // Clone this object to avoid propagating changes made in this test.
    const testSiteGroup = JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
    testSiteGroup.origins[0].engagement = 20;
    testSiteGroup.origins[1].engagement = 30;
    testSiteGroup.origins[2].engagement = 10;
    testSiteGroup.origins[0].usage = 0;
    testSiteGroup.origins[1].usage = 1274;
    testSiteGroup.origins[2].usage = 1274;
    testSiteGroup.origins[0].numCookies = 10;
    testSiteGroup.origins[1].numCookies = 3;
    testSiteGroup.origins[2].numCookies = 1;
    testElement.sortMethod = SortMethod.STORAGE;
    testElement.siteGroup = testSiteGroup;
    flush();
    const collapseChild = testElement.$.originList.get();
    flush();
    const origins = collapseChild.querySelectorAll('.origin-link');
    assertEquals(3, origins.length);
    assertEquals(
        'www.example.com',
        origins[0].querySelector('#originSiteRepresentation').innerText.trim());
    assertEquals(
        'login.example.com',
        origins[1].querySelector('#originSiteRepresentation').innerText.trim());
    assertEquals(
        'example.com',
        origins[2].querySelector('#originSiteRepresentation').innerText.trim());
  });

  test('can be sorted by name', function() {
    // Clone this object to avoid propagating changes made in this test.
    const testSiteGroup = JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
    testSiteGroup.origins[0].engagement = 20;
    testSiteGroup.origins[1].engagement = 30;
    testSiteGroup.origins[2].engagement = 10;
    testSiteGroup.origins[0].usage = 0;
    testSiteGroup.origins[1].usage = 1274;
    testSiteGroup.origins[2].usage = 1274;
    testSiteGroup.origins[0].numCookies = 10;
    testSiteGroup.origins[1].numCookies = 3;
    testSiteGroup.origins[2].numCookies = 1;
    testElement.sortMethod = SortMethod.NAME;
    testElement.siteGroup = testSiteGroup;
    flush();
    const collapseChild = testElement.$.originList.get();
    flush();
    const origins = collapseChild.querySelectorAll('.origin-link');
    assertEquals(3, origins.length);
    assertEquals(
        'example.com',
        origins[0].querySelector('#originSiteRepresentation').innerText.trim());
    assertEquals(
        'login.example.com',
        origins[1].querySelector('#originSiteRepresentation').innerText.trim());
    assertEquals(
        'www.example.com',
        origins[2].querySelector('#originSiteRepresentation').innerText.trim());
  });
});
