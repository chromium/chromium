// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('SiteEntry', function() {
  /**
   * An example eTLD+1 Object with multiple origins grouped under it.
   * @type {!SiteGroup}
   */
  const TEST_MULTIPLE_SITE_GROUP = test_util.createSiteGroup('example.com', [
    'http://example.com',
    'https://www.example.com',
    'https://login.example.com',
  ]);

  /**
   * An example eTLD+1 Object with a single origin in it.
   * @type {!SiteGroup}
   */
  const TEST_SINGLE_SITE_GROUP = test_util.createSiteGroup('foo.com', [
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
    settings.SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
    settings.LocalDataBrowserProxyImpl.instance_ = localDataBrowserProxy;

    PolymerTest.clearBody();
    testElement = document.createElement('site-entry');
    assertTrue(!!testElement);
    document.body.appendChild(testElement);

    toggleButton = testElement.$.toggleButton;
  });

  teardown(function() {
    // The code being tested changes the Route. Reset so that state is not
    // leaked across tests.
    settings.resetRouteForTesting();
  });

  test('displays the correct number of origins', function() {
    testElement.siteGroup = TEST_MULTIPLE_SITE_GROUP;
    Polymer.dom.flush();
    const collapseChild = testElement.$.originList.get();
    Polymer.dom.flush();
    assertEquals(3, collapseChild.querySelectorAll('.list-item').length);
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
        settings.routes.SITE_SETTINGS_SITE_DETAILS.path,
        settings.getCurrentRoute().path);
    assertEquals(
        'https://login.foo.com', settings.getQueryParameters().get('site'));
  });

  test('with multiple origins navigates to Site Details', function() {
    testElement.siteGroup = TEST_MULTIPLE_SITE_GROUP;
    Polymer.dom.flush();
    const collapseChild = testElement.$.originList.get();
    Polymer.dom.flush();
    const originList = collapseChild.querySelectorAll('.list-item');
    assertEquals(3, originList.length);

    // Test clicking on one of these origins takes the user to Site Details,
    // with the correct origin.
    originList[1].click();
    assertEquals(
        settings.routes.SITE_SETTINGS_SITE_DETAILS.path,
        settings.getCurrentRoute().path);
    assertEquals(
        TEST_MULTIPLE_SITE_GROUP.origins[1].origin,
        settings.getQueryParameters().get('site'));
  });

  test('with single origin does not show overflow menu', function() {
    testElement.siteGroup = TEST_SINGLE_SITE_GROUP;
    Polymer.dom.flush();
    const overflowMenuButton = testElement.$.overflowMenuButton;
    assertTrue(overflowMenuButton.closest('.row-aligned').hidden);
  });

  test(
      'moving from grouped to ungrouped does not get stuck in opened state',
      function() {
        // Clone this object to avoid propagating changes made in this test.
        testElement.siteGroup =
            JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
        Polymer.dom.flush();
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
    Polymer.dom.flush();
    const cookiesLabel = testElement.$.cookies;
    assertTrue(cookiesLabel.hidden);
    // When the number of cookies is more than zero, the label appears.
    const testSiteGroup = JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
    const numCookies = 3;
    testSiteGroup.numCookies = numCookies;

    testElement.siteGroup = testSiteGroup;

    Polymer.dom.flush();
    return localDataBrowserProxy.whenCalled('getNumCookiesString')
        .then((args) => {
          assertEquals(3, args);
          assertFalse(cookiesLabel.hidden);
          assertEquals('· 3 cookies', cookiesLabel.textContent.trim());
        });
  });

  test('cookies show for ungrouped entries', function() {
    testElement.siteGroup = TEST_SINGLE_SITE_GROUP;
    Polymer.dom.flush();
    const cookiesLabel = testElement.$.cookies;
    assertTrue(cookiesLabel.hidden);


    const testSiteGroup = JSON.parse(JSON.stringify(TEST_SINGLE_SITE_GROUP));
    const numCookies = 3;

    testSiteGroup.numCookies = numCookies;

    testElement.siteGroup = testSiteGroup;

    Polymer.dom.flush();
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
    Polymer.dom.flush();
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
    Polymer.dom.flush();
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
        Polymer.dom.flush();
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
    Polymer.dom.flush();
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
    Polymer.dom.flush();
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
    Polymer.dom.flush();
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
    testElement.sortMethod = settings.SortMethod.MOST_VISITED;
    testElement.siteGroup = testSiteGroup;
    Polymer.dom.flush();
    const collapseChild = testElement.$.originList.get();
    Polymer.dom.flush();
    const origins = collapseChild.querySelectorAll('.list-item');
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
    testElement.sortMethod = settings.SortMethod.STORAGE;
    testElement.siteGroup = testSiteGroup;
    Polymer.dom.flush();
    const collapseChild = testElement.$.originList.get();
    Polymer.dom.flush();
    const origins = collapseChild.querySelectorAll('.list-item');
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
    testElement.sortMethod = settings.SortMethod.NAME;
    testElement.siteGroup = testSiteGroup;
    Polymer.dom.flush();
    const collapseChild = testElement.$.originList.get();
    Polymer.dom.flush();
    const origins = collapseChild.querySelectorAll('.list-item');
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
