// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageState} from 'chrome://extensions/extensions.js';
import {Dialog, NavigationHelper, Page} from 'chrome://extensions/extensions.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockMethod} from 'chrome://webui-test/mock_controller.js';

/**
 * @return A promise that resolves after the next popstate event.
 */
function getOnPopState(): Promise<void> {
  return new Promise<void>(function(resolve) {
    window.addEventListener('popstate', function listener() {
      window.removeEventListener('popstate', listener);
      // Resolve asynchronously to allow all other listeners to run.
      window.setTimeout(resolve, 0);
    });
  });
}

suite('ExtensionNavigationHelperTest', function() {
  let navigationHelper: NavigationHelper;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    navigationHelper = new NavigationHelper();
  });

  test('Basic', function() {
    const id = 'a'.repeat(32);
    const mock = new MockMethod();

    function changePage(state: PageState) {
      mock.recordCall([state]);
    }

    navigationHelper.addListener(changePage);

    assertDeepEquals({page: Page.LIST}, navigationHelper.getCurrentPage());

    let currentLength = history.length;
    navigationHelper.updateHistory(
        {page: Page.DETAILS, extensionId: id}, false);
    assertEquals(++currentLength, history.length);

    navigationHelper.updateHistory({page: Page.ERRORS, extensionId: id}, false);
    assertEquals(++currentLength, history.length);

    mock.addExpectation({page: Page.DETAILS, extensionId: id});
    const waitForPop = getOnPopState();
    history.back();
    return waitForPop
        .then(() => {
          mock.verifyMock();

          mock.addExpectation({page: Page.LIST});
          const waitForNextPop = getOnPopState();
          history.back();
          return waitForNextPop;
        })
        .then(() => {
          mock.verifyMock();
        });
  });

  test('Conversions', function() {
    const id = 'a'.repeat(32);
    const stateUrlPairs: {[k: string]: {url: string, state: PageState}} = {
      extensions: {
        url: 'chrome://extensions/',
        state: {page: Page.LIST},
      },
      details: {
        url: 'chrome://extensions/?id=' + id,
        state: {page: Page.DETAILS, extensionId: id},
      },
      options: {
        url: 'chrome://extensions/?options=' + id,
        state: {
          page: Page.DETAILS,
          extensionId: id,
          subpage: Dialog.OPTIONS,
        },
      },
      errors: {
        url: 'chrome://extensions/?errors=' + id,
        state: {page: Page.ERRORS, extensionId: id},
      },
      shortcuts: {
        url: 'chrome://extensions/shortcuts',
        state: {page: Page.SHORTCUTS},
      },
      sitePermissions: {
        url: 'chrome://extensions/sitePermissions',
        state: {page: Page.SITE_PERMISSIONS},
      },
      sitePermissionsAllSites: {
        url: 'chrome://extensions/sitePermissions/allSites',
        state: {page: Page.SITE_PERMISSIONS_ALL_SITES},
      },
    };

    // Test url -> state.
    for (const key in stateUrlPairs) {
      const entry = stateUrlPairs[key];
      assertTrue(!!entry);
      history.pushState({}, '', entry.url);
      assertDeepEquals(entry.state, navigationHelper.getCurrentPage(), key);
    }

    // Test state -> url.
    for (const key in stateUrlPairs) {
      const entry = stateUrlPairs[key];
      assertTrue(!!entry);
      navigationHelper.updateHistory(entry.state, false);
      assertEquals(entry.url, location.href, key);
    }
  });

  test('PushAndReplaceState', function() {
    const id1 = 'a'.repeat(32);
    const id2 = 'b'.repeat(32);

    history.pushState({}, '', 'chrome://extensions/');
    assertDeepEquals({page: Page.LIST}, navigationHelper.getCurrentPage());

    let expectedLength = history.length;

    // Navigating to a new page pushes new state.
    navigationHelper.updateHistory(
        {page: Page.DETAILS, extensionId: id1}, false);
    assertEquals(++expectedLength, history.length);

    // Navigating to a subpage (like the options page) just opens a
    // dialog, and shouldn't push new state.
    navigationHelper.updateHistory(
        {page: Page.DETAILS, extensionId: id1, subpage: Dialog.OPTIONS}, false);
    assertEquals(expectedLength, history.length);

    // Navigating away from a subpage also shouldn't push state (it just
    // closes the dialog).
    navigationHelper.updateHistory(
        {page: Page.DETAILS, extensionId: id1}, false);
    assertEquals(expectedLength, history.length);

    // Navigating away should push new state.
    navigationHelper.updateHistory({page: Page.LIST}, false);
    assertEquals(++expectedLength, history.length);

    // Navigating to a subpage of a different page should push state.
    navigationHelper.updateHistory(
        {page: Page.DETAILS, extensionId: id1, subpage: Dialog.OPTIONS}, false);
    assertEquals(++expectedLength, history.length);

    // Navigating away from a subpage to a page for a different item
    // should push state.
    navigationHelper.updateHistory(
        {page: Page.DETAILS, extensionId: id2}, false);
    assertEquals(++expectedLength, history.length);

    // Using replaceWith, which passes true for replaceState should not
    // push state.
    navigationHelper.updateHistory(
        {page: Page.DETAILS, extensionId: id1}, true /* replaceState */);
    assertEquals(expectedLength, history.length);
  });

  test('SupportedRoutes', function() {
    function removeEndSlash(url: string): string {
      const CANONICAL_PATH_REGEX = /([\/-\w]+)\/$/;
      return url.replace(CANONICAL_PATH_REGEX, '$1');
    }

    // If it should not redirect, leave newUrl as undefined.
    function testIfRedirected(url: string, newUrl?: string) {
      history.pushState({}, '', url);
      new NavigationHelper();  // Called for its side-effects.
      assertEquals(
          removeEndSlash(window.location.href), removeEndSlash(newUrl || url));
    }

    testIfRedirected('chrome://extensions');
    testIfRedirected('chrome://extensions/');
    testIfRedirected('chrome://extensions/shortcuts');
    testIfRedirected('chrome://extensions/shortcuts/');
    testIfRedirected('chrome://extensions/fake-route', 'chrome://extensions');
    // Test trailing slash works.

    // Test legacy paths
    testIfRedirected(
        'chrome://extensions/configureCommands',
        'chrome://extensions/shortcuts');
  });
});
