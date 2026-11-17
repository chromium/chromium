// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {SiteEntryElement, SiteGroup} from 'chrome://settings/lazy_load.js';
import {SiteSettingsBrowserProxyImpl, SortMethod} from 'chrome://settings/lazy_load.js';
import {Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isChildVisible, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSiteSettingsBrowserProxy} from './test_site_settings_browser_proxy.js';
import {createOriginInfo, createSiteGroup} from './test_util.js';

suite('SiteEntry', function() {
  /**
   * An example eTLD+1 Object with multiple origins grouped under it.
   */
  function createMultipleSiteGroup(): SiteGroup {
    return createSiteGroup('example.com', 'example.com', [
      'http://example.com',
      'https://www.example.com',
      'https://login.example.com',
    ]);
  }

  /**
   * An example eTLD+1 Object with a single origin in it.
   */
  function createSingleSiteGroup(): SiteGroup {
    return createSiteGroup('foo.com', 'foo.com', [
      'https://login.foo.com',
    ]);
  }

  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestSiteSettingsBrowserProxy;

  /**
   * A site list element created before each test.
   */
  let testElement: SiteEntryElement;

  function getOriginRepresentation(origin: Element): string {
    const representation =
        origin.querySelector<HTMLElement>('#originSiteRepresentation');
    assertTrue(!!representation);
    return representation.textContent.trim();
  }

  // Initialize a site-list before each test.
  setup(function() {
    browserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(browserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('site-entry');
    document.body.appendChild(testElement);
  });

  teardown(function() {
    // The code being tested changes the Route. Reset so that state is not
    // leaked across tests.
    Router.getInstance().resetRouteForTesting();
  });

  test('displays the correct number of origins', async function() {
    testElement.siteGroup = createMultipleSiteGroup();
    await microtasksFinished();
    const collapseChild = testElement.$.originList.get();
    assertEquals(3, collapseChild.querySelectorAll('.origin-link').length);
  });

  test('expands and closes to show more origins', async () => {
    testElement.siteGroup = createMultipleSiteGroup();
    await microtasksFinished();
    assertTrue(isVisible(testElement.$.expandIcon));
    assertTrue(testElement.$.expandIcon.classList.contains('icon-expand-more'));
    assertEquals(
        'false', testElement.$.toggleButton.getAttribute('aria-expanded'));
    assertEquals(
        'false', testElement.$.expandIcon.getAttribute('aria-expanded'));
    const originList = testElement.$.originList.get();
    assertTrue(originList.classList.contains('collapse-closed'));
    assertEquals('true', originList.getAttribute('aria-hidden'));

    testElement.$.toggleButton.click();
    await microtasksFinished();
    assertTrue(testElement.$.expandIcon.classList.contains('icon-expand-less'));
    assertEquals(
        'true', testElement.$.toggleButton.getAttribute('aria-expanded'));
    assertEquals(
        'true', testElement.$.expandIcon.getAttribute('aria-expanded'));
    assertTrue(originList.classList.contains('collapse-opened'));
    assertEquals('false', originList.getAttribute('aria-hidden'));

    testElement.$.toggleButton.click();
    await microtasksFinished();
    assertTrue(testElement.$.expandIcon.classList.contains('icon-expand-more'));
    assertEquals(
        'false', testElement.$.toggleButton.getAttribute('aria-expanded'));
    assertEquals(
        'false', testElement.$.expandIcon.getAttribute('aria-expanded'));
    assertTrue(originList.classList.contains('collapse-closed'));
    assertEquals('true', originList.getAttribute('aria-hidden'));
  });

  test('with single origin navigates to Site Details', async function() {
    testElement.siteGroup = createSingleSiteGroup();
    await microtasksFinished();
    assertFalse(isVisible(testElement.$.expandIcon));
    assertEquals(
        'false', testElement.$.toggleButton.getAttribute('aria-expanded'));
    const originList = testElement.$.originList.get();
    assertTrue(originList.classList.contains('collapse-closed'));
    assertEquals('true', originList.getAttribute('aria-hidden'));

    testElement.$.toggleButton.click();
    assertEquals(
        'false', testElement.$.toggleButton.getAttribute('aria-expanded'));
    assertTrue(originList.classList.contains('collapse-closed'));
    assertEquals('true', originList.getAttribute('aria-hidden'));
    assertEquals(
        routes.SITE_SETTINGS_SITE_DETAILS.path,
        Router.getInstance().getCurrentRoute().path);
    assertEquals(
        'https://login.foo.com',
        Router.getInstance().getQueryParameters().get('site'));
  });

  test('with multiple origins navigates to Site Details', async function() {
    const testSiteGroup = createMultipleSiteGroup();
    testElement.siteGroup = testSiteGroup;
    await microtasksFinished();
    const collapseChild = testElement.$.originList.get();
    const originList =
        collapseChild.querySelectorAll<HTMLElement>('.origin-link');
    assertEquals(3, originList.length);

    // Test clicking on one of these origins takes the user to Site Details,
    // with the correct origin.
    originList[1]!.click();
    assertEquals(
        routes.SITE_SETTINGS_SITE_DETAILS.path,
        Router.getInstance().getCurrentRoute().path);
    assertEquals(
        testSiteGroup.origins[1]!.origin,
        Router.getInstance().getQueryParameters().get('site'));
  });

  test(
      'moving from grouped to ungrouped does not get stuck in opened state',
      async function() {
        testElement.siteGroup = createMultipleSiteGroup();
        await microtasksFinished();
        testElement.$.toggleButton.click();
        await microtasksFinished();
        assertTrue(testElement.$.originList.get().opened);

        // Remove all origins except one, then make sure it's not still
        // expanded.
        const siteGroupUpdated = createMultipleSiteGroup();
        siteGroupUpdated.origins.splice(1);
        testElement.siteGroup = siteGroupUpdated;
        await microtasksFinished();
        assertEquals(1, testElement.siteGroup.origins.length);
        assertFalse(testElement.$.originList.get().opened);
      });

  test('cookies show correctly for grouped entries', async function() {
    testElement.siteGroup = createMultipleSiteGroup();
    await microtasksFinished();
    const cookiesLabel = testElement.$.cookies;
    assertFalse(isVisible(cookiesLabel));
    // When the number of cookies is more than zero, the label appears.
    const testSiteGroup = createMultipleSiteGroup();
    const numCookies = 3;
    testSiteGroup.numCookies = numCookies;

    testElement.siteGroup = testSiteGroup;
    const args = await browserProxy.whenCalled('getNumCookiesString');
    await microtasksFinished();
    assertEquals(3, args);
    assertTrue(isVisible(cookiesLabel));
    assertEquals('· 3 cookies', cookiesLabel.textContent.trim());
  });

  test('cookies show for ungrouped entries', async function() {
    testElement.siteGroup = createSingleSiteGroup();
    await microtasksFinished();
    const cookiesLabel = testElement.$.cookies;
    assertFalse(isVisible(cookiesLabel));

    const testSiteGroup = createSingleSiteGroup();
    const numCookies = 3;

    testSiteGroup.numCookies = numCookies;

    testElement.siteGroup = testSiteGroup;
    const args = await browserProxy.whenCalled('getNumCookiesString');
    await microtasksFinished();
    assertEquals(3, args);
    assertTrue(isVisible(cookiesLabel));
    assertEquals('· 3 cookies', cookiesLabel.textContent.trim());
  });

  test('data usage shown correctly for grouped entries', async function() {
    const testSiteGroup = createMultipleSiteGroup();
    const numBytes1 = 74622;
    const numBytes2 = 1274;
    const numBytes3 = 0;
    testSiteGroup.origins[0]!.usage = numBytes1;
    testSiteGroup.origins[1]!.usage = numBytes2;
    testSiteGroup.origins[2]!.usage = numBytes3;
    testElement.siteGroup = testSiteGroup;
    const args = await browserProxy.whenCalled('getFormattedBytes');
    await microtasksFinished();
    const sumBytes = numBytes1 + numBytes2 + numBytes3;
    assertEquals(sumBytes, args);
    const dataUnit = testElement.shadowRoot.querySelector<HTMLElement>(
        '#displayName .data-unit');
    assertTrue(!!dataUnit);
    assertEquals(`${sumBytes} B`, dataUnit.textContent.trim());
  });

  test('data usage shown correctly for ungrouped entries', async function() {
    const testSiteGroup = createSingleSiteGroup();
    const numBytes = 74622;
    testSiteGroup.origins[0]!.usage = numBytes;
    testElement.siteGroup = testSiteGroup;
    const args = await browserProxy.whenCalled('getFormattedBytes');
    await microtasksFinished();
    assertEquals(numBytes, args);
    const dataUnit = testElement.shadowRoot.querySelector<HTMLElement>(
        '#displayName .data-unit');
    assertTrue(!!dataUnit);
    assertEquals(`${numBytes} B`, dataUnit.textContent.trim());
  });

  test(
      'large number data usage shown correctly for grouped entries',
      async function() {
        const testSiteGroup = createMultipleSiteGroup();
        const numBytes1 = 2000000000;
        const numBytes2 = 10000000000;
        const numBytes3 = 7856;
        testSiteGroup.origins[0]!.usage = numBytes1;
        testSiteGroup.origins[1]!.usage = numBytes2;
        testSiteGroup.origins[2]!.usage = numBytes3;
        testElement.siteGroup = testSiteGroup;
        const args = await browserProxy.whenCalled('getFormattedBytes');
        await microtasksFinished();
        const sumBytes = numBytes1 + numBytes2 + numBytes3;
        assertEquals(sumBytes, args);
        const dataUnit = testElement.shadowRoot.querySelector<HTMLElement>(
            '#displayName .data-unit');
        assertTrue(!!dataUnit);
        assertEquals(`${sumBytes} B`, dataUnit.textContent.trim());
      });

  test('favicon with www.etld+1 chosen for site group', async function() {
    const testSiteGroup = createMultipleSiteGroup();
    testSiteGroup.origins[0]!.usage = 0;
    testSiteGroup.origins[1]!.usage = 1274;
    testSiteGroup.origins[2]!.usage = 74622;
    testElement.siteGroup = testSiteGroup;
    await microtasksFinished();
    const favicon = testElement.$.collapseParent.querySelector('site-favicon');
    assertTrue(!!favicon);
    assertEquals('https://www.example.com', favicon.url);
  });

  test('favicon with largest storage chosen for site group', async function() {
    const testSiteGroup = createMultipleSiteGroup();
    testSiteGroup.origins[0]!.usage = 0;
    testSiteGroup.origins[1]!.usage = 1274;
    testSiteGroup.origins[2]!.usage = 74622;
    testSiteGroup.origins[1]!.origin = 'https://abc.example.com';
    testElement.siteGroup = testSiteGroup;
    await microtasksFinished();
    const favicon = testElement.$.collapseParent.querySelector('site-favicon');
    assertTrue(!!favicon);
    assertEquals('https://login.example.com', favicon.url);
  });

  test(
      'favicon with largest cookies number chosen for site group',
      async function() {
        const testSiteGroup = createMultipleSiteGroup();
        testSiteGroup.origins[0]!.usage = 0;
        testSiteGroup.origins[1]!.usage = 1274;
        testSiteGroup.origins[2]!.usage = 1274;
        testSiteGroup.origins[0]!.numCookies = 10;
        testSiteGroup.origins[1]!.numCookies = 3;
        testSiteGroup.origins[2]!.numCookies = 1;
        testSiteGroup.origins[1]!.origin = 'https://abc.example.com';
        testElement.siteGroup = testSiteGroup;
        await microtasksFinished();
        const favicon =
            testElement.$.collapseParent.querySelector('site-favicon');
        assertTrue(!!favicon);
        assertEquals('https://abc.example.com', favicon.url);
      });

  test('can be sorted by most visited', async function() {
    const testSiteGroup = createMultipleSiteGroup();
    testSiteGroup.origins[0]!.engagement = 20;
    testSiteGroup.origins[1]!.engagement = 30;
    testSiteGroup.origins[2]!.engagement = 10;
    testSiteGroup.origins[0]!.usage = 0;
    testSiteGroup.origins[1]!.usage = 1274;
    testSiteGroup.origins[2]!.usage = 1274;
    testSiteGroup.origins[0]!.numCookies = 10;
    testSiteGroup.origins[1]!.numCookies = 3;
    testSiteGroup.origins[2]!.numCookies = 1;
    testElement.sortMethod = SortMethod.MOST_VISITED;
    testElement.siteGroup = testSiteGroup;
    await microtasksFinished();
    const collapseChild = testElement.$.originList.get();
    const origins = collapseChild.querySelectorAll('.origin-link');
    assertEquals(3, origins.length);
    assertEquals('www.example.com', getOriginRepresentation(origins[0]!));
    assertEquals('example.com', getOriginRepresentation(origins[1]!));
    assertEquals('login.example.com', getOriginRepresentation(origins[2]!));
  });

  test('can be sorted by storage', async function() {
    const testSiteGroup = createMultipleSiteGroup();
    testSiteGroup.origins[0]!.engagement = 20;
    testSiteGroup.origins[1]!.engagement = 30;
    testSiteGroup.origins[2]!.engagement = 10;
    testSiteGroup.origins[0]!.usage = 0;
    testSiteGroup.origins[1]!.usage = 1274;
    testSiteGroup.origins[2]!.usage = 1274;
    testSiteGroup.origins[0]!.numCookies = 10;
    testSiteGroup.origins[1]!.numCookies = 3;
    testSiteGroup.origins[2]!.numCookies = 1;
    testElement.sortMethod = SortMethod.STORAGE;
    testElement.siteGroup = testSiteGroup;
    await microtasksFinished();
    const collapseChild = testElement.$.originList.get();
    const origins = collapseChild.querySelectorAll('.origin-link');
    assertEquals(3, origins.length);
    assertEquals('www.example.com', getOriginRepresentation(origins[0]!));
    assertEquals('login.example.com', getOriginRepresentation(origins[1]!));
    assertEquals('example.com', getOriginRepresentation(origins[2]!));
  });

  test('can be sorted by name', async function() {
    const testSiteGroup = createMultipleSiteGroup();
    testSiteGroup.origins[0]!.engagement = 20;
    testSiteGroup.origins[1]!.engagement = 30;
    testSiteGroup.origins[2]!.engagement = 10;
    testSiteGroup.origins[0]!.usage = 0;
    testSiteGroup.origins[1]!.usage = 1274;
    testSiteGroup.origins[2]!.usage = 1274;
    testSiteGroup.origins[0]!.numCookies = 10;
    testSiteGroup.origins[1]!.numCookies = 3;
    testSiteGroup.origins[2]!.numCookies = 1;
    testElement.sortMethod = SortMethod.NAME;
    testElement.siteGroup = testSiteGroup;
    await microtasksFinished();
    const collapseChild = testElement.$.originList.get();
    const origins = collapseChild.querySelectorAll('.origin-link');
    assertEquals(3, origins.length);
    assertEquals('example.com', getOriginRepresentation(origins[0]!));
    assertEquals('login.example.com', getOriginRepresentation(origins[1]!));
    assertEquals('www.example.com', getOriginRepresentation(origins[2]!));
  });

  test('remove site fires correct event for individual site', async function() {
    testElement.siteGroup = createMultipleSiteGroup();
    await microtasksFinished();

    const collapseChild = testElement.$.originList.get();
    const originList = collapseChild.querySelectorAll('.hr');
    assertEquals(3, originList.length);

    for (let i = 0; i < originList.length; i++) {
      const siteRemoved = eventToPromise('remove-site', testElement);
      const removeOriginButton =
          originList[i]!.querySelector<HTMLElement>('#removeOriginButton');
      assertTrue(!!removeOriginButton);
      removeOriginButton.click();
      const siteRemovedEvent = await siteRemoved;

      const {actionScope, index, origin} = siteRemovedEvent.detail;
      assertEquals('origin', actionScope);
      assertEquals(testElement.listIndex, index);
      assertEquals(testElement.siteGroup.origins[i]!.origin, origin);
    }
  });

  test('remove site fires correct event for site group', async function() {
    testElement.siteGroup = createMultipleSiteGroup();
    await microtasksFinished();

    const siteRemoved = eventToPromise('remove-site', testElement);
    const removeSiteButton =
        testElement.shadowRoot.querySelector<HTMLElement>('#removeSiteButton');
    assertTrue(!!removeSiteButton);
    removeSiteButton.click();
    const siteRemovedEvent = await siteRemoved;

    const {actionScope, index, origin} = siteRemovedEvent.detail;

    // Site groups are removed exclusively based on their index.
    assertEquals(undefined, actionScope);
    assertEquals(testElement.listIndex, index);
    assertEquals(undefined, origin);
  });

  test('partitioned entry interaction', async function() {
    const testSiteGroup = createMultipleSiteGroup();

    // Add a partitioned entry for an unrelated origin.
    testSiteGroup.origins.push(
        createOriginInfo('wwww.unrelated.com', {isPartitioned: true}));

    testElement.siteGroup = testSiteGroup;
    await microtasksFinished();
    const collapseChild = testElement.$.originList.get();
    testElement.$.toggleButton.click();
    await microtasksFinished();

    const originList = collapseChild.querySelectorAll('.hr');
    assertEquals(4, originList.length);

    // Partitioned entries should not be displaying a link arrow, while
    // unpartitioned entries should.
    assertTrue(isChildVisible(
        originList[0]!, 'cr-icon-button', /*checkLightDom=*/ true));
    assertFalse(isChildVisible(
        originList[3]!, 'cr-icon-button', /*checkLightDom=*/ true));

    // Removing a partitioned entry should fire the appropriate event.
    const siteRemoved = eventToPromise('remove-site', testElement);
    const removeOriginButton =
        originList[3]!.querySelector<HTMLElement>('#removeOriginButton');
    assertTrue(!!removeOriginButton);
    removeOriginButton.click();
    const siteRemovedEvent = await siteRemoved;

    const args = siteRemovedEvent.detail;
    const {actionScope, index, origin, isPartitioned} = args;
    assertEquals('origin', actionScope);
    assertEquals(testElement.listIndex, index);
    assertEquals(testElement.siteGroup.origins[3]!.origin, origin);
    assertTrue(isPartitioned);
  });

  test('partitioned entry prevents collapse', async function() {
    // If a siteGroup has a partitioned entry, even if it is the only entry,
    // it should keep the site entry as a top level + collapse list.
    const testSingleSite = createSingleSiteGroup();
    testSingleSite.origins[0]!.isPartitioned = true;

    testElement.siteGroup = testSingleSite;
    await microtasksFinished();
    const collapseChild = testElement.$.originList.get();

    // The toggle button should expand the collapse, rather than navigate.
    const startingRoute = Router.getInstance().getCurrentRoute();
    testElement.$.toggleButton.click();
    await microtasksFinished();
    assertEquals(startingRoute, Router.getInstance().getCurrentRoute());

    const originList = collapseChild.querySelectorAll('.hr');
    assertEquals(1, originList.length);
  });

  test('unpartitioned entry remains collapsed', async function() {
    // Check that a single origin containing unpartitioned storage only is
    // correctly collapsed.
    testElement.siteGroup = createSingleSiteGroup();
    await microtasksFinished();
    const collapseChild = testElement.$.originList.get();

    testElement.$.toggleButton.click();
    assertFalse(collapseChild.opened);

    // Clicking the toggleButton should navigate the page away, as there is
    // only one entry.
    testElement.$.toggleButton.click();
    assertEquals(
        routes.SITE_SETTINGS_SITE_DETAILS.path,
        Router.getInstance().getCurrentRoute().path);
  });

  test('extension site group is shown correctly', async function() {
    const extensionSiteGroup = createSingleSiteGroup();
    extensionSiteGroup.displayName = 'Test Extension';
    extensionSiteGroup.origins[0]!.origin =
        'chrome-extension://mhabknllooicelmdboebjilbohdbihln';
    testElement.siteGroup = extensionSiteGroup;
    await microtasksFinished();

    // Check if the extension name shown correctly.
    const urlDirectionality =
        testElement.$.collapseParent.querySelector('.url-directionality');
    assertTrue(!!urlDirectionality);
    assertEquals('Test Extension', urlDirectionality.textContent.trim());

    // Check if the extension id is shown correctly.
    assertTrue(isVisible(testElement.$.extensionIdDescription));
    assertEquals(
        '· ID: mhabknllooicelmdboebjilbohdbihln',
        testElement.$.extensionIdDescription.textContent.trim());
  });

  test('changing sortMethod preserves focus on expandIcon', async function() {
    testElement.siteGroup = createMultipleSiteGroup();
    await microtasksFinished();

    testElement.$.expandIcon.focus();
    assertEquals(
        testElement.$.expandIcon, testElement.shadowRoot.activeElement);

    testElement.sortMethod = SortMethod.NAME;
    await microtasksFinished();

    assertEquals(
        testElement.$.expandIcon, testElement.shadowRoot.activeElement);
  });
});
