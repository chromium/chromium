// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Test suite for testing route navigation logic of MainPageMixin, for which
 * <main-page-container> is the primary element.
 *
 * Assumes kOsSettingsRevampWayfinding feature flag is enabled.
 */

import 'chrome://os-settings/os_settings.js';

import {createPageAvailabilityForTesting, CrSettingsPrefs, Router, routes, routesMojom, setContactManagerForTesting, setNearbyShareSettingsForTesting} from 'chrome://os-settings/os_settings.js';
import {assertEquals, assertNotEquals, assertNull} from 'chrome://webui-test/chai_assert.js';
import {FakeContactManager} from 'chrome://webui-test/nearby_share/shared/fake_nearby_contact_manager.js';
import {FakeNearbyShareSettings} from 'chrome://webui-test/nearby_share/shared/fake_nearby_share_settings.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

const {Section} = routesMojom;

suite('<main-page-container> Route Navigation', () => {
  let mainPageContainer;
  let prefElement;
  let fakeContactManager;
  let fakeNearbyShareSettings;

  async function initElement() {
    const element = document.createElement('main-page-container');
    element.prefs = prefElement.prefs;
    element.pageAvailability = createPageAvailabilityForTesting();
    document.body.appendChild(element);
    await flushTasks();
    return element;
  }

  suiteSetup(async () => {
    // Simulate feature flag enabled for CSS styling purposes.
    document.body.classList.add('revamp-wayfinding-enabled');

    // Setup test fixtures
    fakeContactManager = new FakeContactManager();
    setContactManagerForTesting(fakeContactManager);
    fakeNearbyShareSettings = new FakeNearbyShareSettings();
    setNearbyShareSettingsForTesting(fakeNearbyShareSettings);

    prefElement = document.createElement('settings-prefs');
    await CrSettingsPrefs.initialized;
  });

  setup(async () => {
    Router.getInstance().navigateTo(routes.BASIC);
    mainPageContainer = await initElement();
  });

  teardown(() => {
    mainPageContainer.remove();
    CrSettingsPrefs.resetForTesting();
    Router.getInstance().resetRouteForTesting();
  });

  /**
   * Asserts the page with the given |section| is the only visible page by
   * checking:
   * - Only one page is marked active
   * - Active page does not have style "display: none"
   * - Inactive pages have style "display: none"
   */
  function assertOnlyVisiblePage(section) {
    const pages =
        mainPageContainer.shadowRoot.querySelectorAll('page-displayer');
    let numActive = 0;

    for (const page of pages) {
      const displayStyle = getComputedStyle(page).display;
      if (page.hasAttribute('active')) {
        numActive++;
        assertNotEquals('none', displayStyle);
        assertEquals(section, page.section);
      } else {
        assertEquals('none', displayStyle);
      }
    }

    assertEquals(1, numActive);
  }

  /**
   * Asserts the page with the given |section| is focused.
   */
  function assertPageIsFocused(section) {
    const page = mainPageContainer.shadowRoot.querySelector(
        `page-displayer[section="${section}"`);
    assertEquals(page, mainPageContainer.shadowRoot.activeElement);
  }

  /**
   * Executes |wrappedFn| and then waits for the show-container event to
   * fire before proceeding.
   */
  async function runAndWaitForContainerShown(wrappedFn) {
    const showContainerPromise = eventToPromise('show-container', window);
    wrappedFn();
    await flushTasks();
    await showContainerPromise;
  }

  test('Network page is initially visible but not focused', async () => {
    assertOnlyVisiblePage(Section.kNetwork);
    assertNull(mainPageContainer.shadowRoot.activeElement);
  });

  suite('From Root', () => {
    test('to Page should activate and focus that page', async () => {
      // Simulate navigating from root to Bluetooth page
      await runAndWaitForContainerShown(() => {
        Router.getInstance().navigateTo(routes.BLUETOOTH);
      });

      assertOnlyVisiblePage(Section.kBluetooth);
      assertPageIsFocused(Section.kBluetooth);
    });

    test('to Subpage should activate parent (top-level) page', async () => {
      // Simulate navigating from root to Bluetooth subpage
      await runAndWaitForContainerShown(() => {
        Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICES);
      });

      assertOnlyVisiblePage(Section.kBluetooth);
    });

    test('to Root should show Network page', async () => {
      // Simulate root page with search query
      Router.getInstance().navigateTo(
          routes.BASIC, new URLSearchParams('search=bluetooth'));

      // Simulate clearing search
      await runAndWaitForContainerShown(() => {
        Router.getInstance().navigateTo(
            routes.BASIC, /*dynamicParameters=*/ undefined,
            /*removeSearch=*/ true);
      });

      assertOnlyVisiblePage(Section.kNetwork);
    });
  });

  suite('From Page', () => {
    setup(async () => {
      // Simulate current route is A11y page
      Router.getInstance().navigateTo(routes.OS_ACCESSIBILITY);
    });

    test('to another Page should activate and focus that page', async () => {
      // Simulate navigating from A11y page to Bluetooth page
      await runAndWaitForContainerShown(() => {
        Router.getInstance().navigateTo(routes.BLUETOOTH);
      });

      assertOnlyVisiblePage(Section.kBluetooth);
      assertPageIsFocused(Section.kBluetooth);
    });

    test('to Subpage should activate parent (top-level) page', async () => {
      // Simulate navigating from A11y page to A11y display subpage
      await runAndWaitForContainerShown(() => {
        Router.getInstance().navigateTo(routes.A11Y_DISPLAY_AND_MAGNIFICATION);
      });

      assertOnlyVisiblePage(Section.kAccessibility);
    });

    test('to Root should activate Network page', async () => {
      // Simulate navigating from A11y page to root
      await runAndWaitForContainerShown(() => {
        Router.getInstance().navigateTo(routes.BASIC);
      });

      assertOnlyVisiblePage(Section.kNetwork);
    });
  });
});
