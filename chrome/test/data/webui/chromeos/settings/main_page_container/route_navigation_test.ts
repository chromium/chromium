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

import {AccountManagerBrowserProxyImpl} from 'chrome://os-settings/lazy_load.js';
import {createPageAvailabilityForTesting, CrSettingsPrefs, MainPageContainerElement, Router, routes, routesMojom, setContactManagerForTesting, setNearbyShareSettingsForTesting, SettingsPrefsElement} from 'chrome://os-settings/os_settings.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeContactManager} from 'chrome://webui-test/chromeos/nearby_share/shared/fake_nearby_contact_manager.js';
import {FakeNearbyShareSettings} from 'chrome://webui-test/chromeos/nearby_share/shared/fake_nearby_share_settings.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestAccountManagerBrowserProxy} from '../os_people_page/test_account_manager_browser_proxy.js';

const {Section} = routesMojom;

suite('<main-page-container> Route Navigation', () => {
  let mainPageContainer: MainPageContainerElement;
  let prefElement: SettingsPrefsElement;
  let fakeContactManager: FakeContactManager;
  let fakeNearbyShareSettings: FakeNearbyShareSettings;
  let browserProxy: TestAccountManagerBrowserProxy;

  suiteSetup(async () => {
    // Simulate feature flag enabled for CSS styling purposes.
    document.body.classList.add('revamp-wayfinding-enabled');

    // Setup test fixtures
    fakeContactManager = new FakeContactManager();
    setContactManagerForTesting(fakeContactManager);
    fakeNearbyShareSettings = new FakeNearbyShareSettings();
    setNearbyShareSettingsForTesting(fakeNearbyShareSettings);

    browserProxy = new TestAccountManagerBrowserProxy();
    AccountManagerBrowserProxyImpl.setInstanceForTesting(browserProxy);

    prefElement = document.createElement('settings-prefs');
    await CrSettingsPrefs.initialized;
  });

  async function initElement(): Promise<MainPageContainerElement> {
    const element = document.createElement('main-page-container');
    element.prefs = prefElement.prefs!;
    element.pageAvailability = createPageAvailabilityForTesting();
    document.body.appendChild(element);
    await flushTasks();
    return element;
  }

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
   * Asserts the page with the given |section| is the only visible page.
   */
  function assertIsOnlyVisiblePage(section: routesMojom.Section): void {
    const pages =
        mainPageContainer.shadowRoot!.querySelectorAll('page-displayer');
    for (const page of pages) {
      if (page.section === section) {
        assertTrue(isVisible(page));
      } else {
        assertFalse(isVisible(page));
      }
    }
  }

  /**
   * Asserts the page with the given |section| is the only page marked active.
   */
  function assertIsOnlyActivePage(section: routesMojom.Section): void {
    const pages =
        mainPageContainer.shadowRoot!.querySelectorAll('page-displayer');
    for (const page of pages) {
      if (page.section === section) {
        assertTrue(page.active);
      } else {
        assertFalse(page.active);
      }
    }
  }

  /**
   * Asserts the page with the given |section| is focused.
   */
  function assertPageIsFocused(section: routesMojom.Section): void {
    const page = mainPageContainer.shadowRoot!.querySelector(
        `page-displayer[section="${section}"`);
    assertEquals(page, mainPageContainer.shadowRoot!.activeElement);
  }

  /**
   * Executes |wrappedFn| and then waits for the show-container event to
   * fire before proceeding.
   */
  async function runAndWaitForContainerShown(wrappedFn: () => void):
      Promise<void> {
    const showContainerPromise = eventToPromise('show-container', window);
    wrappedFn();
    await flushTasks();
    await showContainerPromise;
  }

  test('Network page is initially visible but not focused', () => {
    assertIsOnlyActivePage(Section.kNetwork);
    assertIsOnlyVisiblePage(Section.kNetwork);
    assertNull(mainPageContainer.shadowRoot!.activeElement);
  });

  test('Advanced page is not directly navigable', () => {
    Router.getInstance().navigateTo(routes.ADVANCED);

    // Should redirect to BASIC route
    assertEquals(routes.BASIC, Router.getInstance().currentRoute);
  });

  suite('From Root', () => {
    test('to Page should show and focus that page', async () => {
      // Simulate navigating from root to Personalization page
      await runAndWaitForContainerShown(() => {
        Router.getInstance().navigateTo(routes.PERSONALIZATION);
      });

      assertIsOnlyActivePage(Section.kPersonalization);
      assertIsOnlyVisiblePage(Section.kPersonalization);
      assertPageIsFocused(Section.kPersonalization);
    });

    test('to Subpage should activate parent (top-level) page', async () => {
      // Simulate navigating from root to Bluetooth devices subpage
      await runAndWaitForContainerShown(() => {
        Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICES);
      });

      assertIsOnlyActivePage(Section.kBluetooth);
    });

    test('to Root should show Network page', async () => {
      // Simulate root page with search query
      Router.getInstance().navigateTo(
          routes.BASIC, new URLSearchParams('search=bluetooth'));
      await flushTasks();

      // Simulate clearing search
      await runAndWaitForContainerShown(() => {
        Router.getInstance().navigateTo(
            routes.BASIC, /*dynamicParameters=*/ undefined,
            /*removeSearch=*/ true);
      });

      assertIsOnlyActivePage(Section.kNetwork);
      assertIsOnlyVisiblePage(Section.kNetwork);
    });
  });

  suite('From Page', () => {
    setup(() => {
      // Simulate current route is A11y page
      Router.getInstance().navigateTo(routes.OS_ACCESSIBILITY);
    });

    test('to another Page should show and focus that page', async () => {
      // Simulate navigating from A11y page to Personalization page
      await runAndWaitForContainerShown(() => {
        Router.getInstance().navigateTo(routes.PERSONALIZATION);
      });

      assertIsOnlyActivePage(Section.kPersonalization);
      assertIsOnlyVisiblePage(Section.kPersonalization);
      assertPageIsFocused(Section.kPersonalization);
    });

    test('to Subpage should activate parent (top-level) page', async () => {
      // Simulate navigating from A11y page to A11y display subpage
      await runAndWaitForContainerShown(() => {
        Router.getInstance().navigateTo(routes.A11Y_DISPLAY_AND_MAGNIFICATION);
      });

      assertIsOnlyActivePage(Section.kAccessibility);
    });

    test('to Root should show Network page', async () => {
      // Simulate navigating from A11y page to root
      await runAndWaitForContainerShown(() => {
        Router.getInstance().navigateTo(routes.BASIC);
      });

      assertIsOnlyActivePage(Section.kNetwork);
      assertIsOnlyVisiblePage(Section.kNetwork);
    });
  });

  suite('From Subpage', () => {
    test('to Root should show Network page', async () => {
      // Simulate current route is Display subpage
      Router.getInstance().navigateTo(routes.DISPLAY);
      await flushTasks();

      // Simulate navigating from subpage back to root
      await runAndWaitForContainerShown(() => {
        Router.getInstance().navigateTo(routes.BASIC);
      });

      assertIsOnlyActivePage(Section.kNetwork);
      assertIsOnlyVisiblePage(Section.kNetwork);
    });

    test(
        'to different top-level Page via menu item should show that page',
        async () => {
          // Simulate current route is Display subpage (under Device page)
          Router.getInstance().navigateTo(routes.DISPLAY);
          await flushTasks();

          // Simulate navigating from subpage to Personalization page
          await runAndWaitForContainerShown(() => {
            Router.getInstance().navigateTo(routes.PERSONALIZATION);
          });

          assertIsOnlyActivePage(Section.kPersonalization);
          assertIsOnlyVisiblePage(Section.kPersonalization);
          assertPageIsFocused(Section.kPersonalization);
        });

    test(
        'to different top-level Page via back button should show that page',
        async () => {
          // Simulate current route is Personalization page
          Router.getInstance().navigateTo(routes.PERSONALIZATION);
          await flushTasks();

          // Simulate navigating to Display subpage (under Device page)
          Router.getInstance().navigateTo(routes.DISPLAY);
          await flushTasks();

          // Simulate navigating to Personalization page via back navigation
          await runAndWaitForContainerShown(() => {
            Router.getInstance().navigateToPreviousRoute();
          });

          assertIsOnlyActivePage(Section.kPersonalization);
          assertIsOnlyVisiblePage(Section.kPersonalization);
          assertPageIsFocused(Section.kPersonalization);
        });
  });
});
