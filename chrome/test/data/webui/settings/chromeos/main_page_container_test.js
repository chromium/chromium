// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {createPageAvailabilityForTesting, CrSettingsPrefs, Router, routes, setContactManagerForTesting, setNearbyShareSettingsForTesting} from 'chrome://os-settings/os_settings.js';
import {setBluetoothConfigForTesting} from 'chrome://resources/ash/common/bluetooth/cros_bluetooth_config.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeBluetoothConfig} from 'chrome://webui-test/cr_components/chromeos/bluetooth/fake_bluetooth_config.js';
import {FakeContactManager} from 'chrome://webui-test/nearby_share/shared/fake_nearby_contact_manager.js';
import {FakeNearbyShareSettings} from 'chrome://webui-test/nearby_share/shared/fake_nearby_share_settings.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('<main-page-container>', function() {
  /** @type {?MainPageContainerElement} */
  let mainPageContainer = null;

  /** @type {?SettingsPrefsElement} */
  let prefElement = null;

  /** @type {!FakeContactManager} */
  let fakeContactManager = null;
  /** @type {!FakeNearbyShareSettings} */
  let fakeSettings = null;

  suiteSetup(async function() {
    fakeContactManager = new FakeContactManager();
    setContactManagerForTesting(fakeContactManager);
    fakeContactManager.setupContactRecords();

    fakeSettings = new FakeNearbyShareSettings();
    setNearbyShareSettingsForTesting(fakeSettings);

    // Using the real CrosBluetoothConfig will crash due to no SessionManager.
    setBluetoothConfigForTesting(new FakeBluetoothConfig());

    PolymerTest.clearBody();
    prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);
    await CrSettingsPrefs.initialized;
  });

  /** @return {MainPageContainerElement} */
  function init() {
    const element = document.createElement('main-page-container');
    element.prefs = prefElement.prefs;
    element.pageAvailability = createPageAvailabilityForTesting();
    document.body.appendChild(element);
    flush();
    return element;
  }

  suite('Page availability', () => {
    suiteSetup(async () => {
      Router.getInstance().navigateTo(routes.BASIC);
      mainPageContainer = init();

      const idleRender =
          mainPageContainer.shadowRoot.querySelector('settings-idle-load');
      await idleRender.get();
    });

    suiteTeardown(() => {
      mainPageContainer.remove();
      CrSettingsPrefs.resetForTesting();
      Router.getInstance().resetRouteForTesting();
    });

    [
        // Basic pages
        {
          pageName: 'internet',
          elementName: 'settings-internet-page',
        },
        {
          pageName: 'bluetooth',
          elementName: 'os-settings-bluetooth-page',
        },
        {
          pageName: 'multidevice',
          elementName: 'settings-multidevice-page',
        },
        {
          pageName: 'kerberos',
          elementName: 'settings-kerberos-page',
        },
        {
          pageName: 'osPeople',
          elementName: 'os-settings-people-page',
        },
        {
          pageName: 'device',
          elementName: 'settings-device-page',
        },
        {
          pageName: 'personalization',
          elementName: 'settings-personalization-page',
        },
        {
          pageName: 'osSearch',
          elementName: 'os-settings-search-page',
        },
        {
          pageName: 'osPrivacy',
          elementName: 'os-settings-privacy-page',
        },
        {
          pageName: 'apps',
          elementName: 'os-settings-apps-page',
        },
        {
          pageName: 'osAccessibility',
          elementName: 'os-settings-a11y-page',
        },

        // Advanced section pages
        {
          pageName: 'dateTime',
          elementName: 'settings-date-time-page',
        },
        {
          pageName: 'osLanguages',
          elementName: 'os-settings-languages-section',
        },
        {
          pageName: 'files',
          elementName: 'os-settings-files-page',
        },
        {
          pageName: 'osPrinting',
          elementName: 'os-settings-printing-page',
        },
        {
          pageName: 'crostini',
          elementName: 'settings-crostini-page',
        },
        {
          pageName: 'osReset',
          elementName: 'os-settings-reset-page',
        },
    ].forEach(({pageName, elementName}) => {
      test(`${pageName} page is controlled by pageAvailability`, () => {
        // Make page available
        mainPageContainer.pageAvailability = {
          ...mainPageContainer.pageAvailability,
          [pageName]: true,
        };
        flush();

        let pageElement =
            mainPageContainer.shadowRoot.querySelector(elementName);
        assertTrue(!!pageElement, `<${elementName}> should exist.`);

        // Make page unavailable
        mainPageContainer.pageAvailability = {
          ...mainPageContainer.pageAvailability,
          [pageName]: false,
        };
        flush();

        pageElement = mainPageContainer.shadowRoot.querySelector(elementName);
        assertEquals(null, pageElement, `<${elementName}> should not exist.`);
      });
    });
  });

  suite('Revamp: Wayfinding', () => {
    suite('when enabled', () => {
      suiteSetup(async () => {
        loadTimeData.overrideValues({isRevampWayfindingEnabled: true});
        Router.getInstance().navigateTo(routes.BASIC);
        mainPageContainer = init();
      });

      suiteTeardown(() => {
        mainPageContainer.remove();
        CrSettingsPrefs.resetForTesting();
        Router.getInstance().resetRouteForTesting();
      });

      setup(() => {
        Router.getInstance().navigateTo(routes.BASIC);
      });

      test('advanced toggle should not render', () => {
        const advancedToggle =
            mainPageContainer.shadowRoot.querySelector('#advancedToggle');
        assertNull(advancedToggle);
      });

      suite('Route navigations', () => {
        function queryActivePages() {
          return mainPageContainer.shadowRoot.querySelectorAll(
              'os-settings-section[active]');
        }

        suite('From Root', () => {
          test('to Page should result in only one active page', async () => {
            // Simulate navigating from root to Network page
            const navigationCompletePromise =
                eventToPromise('show-container', window);
            Router.getInstance().navigateTo(routes.INTERNET);
            await navigationCompletePromise;

            const activePages = queryActivePages();
            assertEquals(1, activePages.length);
            assertEquals('internet', activePages[0].section);
          });

          test('to Subpage should result in only one active page', async () => {
            // Simulate navigating from root to Bluetooth subpage
            const navigationCompletePromise =
                eventToPromise('show-container', window);
            Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICES);
            await navigationCompletePromise;

            const activePages = queryActivePages();
            assertEquals(1, activePages.length);
            assertEquals('bluetooth', activePages[0].section);
          });
        });

        suite('From Page', () => {
          test(
              'to another Page should result in only one active page',
              async () => {
                // Simulate navigating from Network page to Bluetooth page
                Router.getInstance().navigateTo(routes.INTERNET);
                const navigationCompletePromise =
                    eventToPromise('show-container', window);
                Router.getInstance().navigateTo(routes.BLUETOOTH);
                await navigationCompletePromise;

                const activePages = queryActivePages();
                assertEquals(1, activePages.length);
                assertEquals('bluetooth', activePages[0].section);
              });

          test('to Subpage should result in only one active page', async () => {
            // Simulate navigating from A11y page to A11y display subpage
            Router.getInstance().navigateTo(routes.OS_ACCESSIBILITY);
            const navigationCompletePromise =
                eventToPromise('show-container', window);
            Router.getInstance().navigateTo(
                routes.A11Y_DISPLAY_AND_MAGNIFICATION);
            await navigationCompletePromise;

            const activePages = queryActivePages();
            assertEquals(1, activePages.length);
            assertEquals('osAccessibility', activePages[0].section);
          });

          test('to Root should result in only one active page', async () => {
            // Simulate navigating from Network page to root
            Router.getInstance().navigateTo(routes.INTERNET);
            const navigationCompletePromise =
                eventToPromise('show-container', window);
            Router.getInstance().navigateTo(routes.BASIC);
            await navigationCompletePromise;

            const activePages = queryActivePages();
            assertEquals(1, activePages.length);
            assertEquals('internet', activePages[0].section);
          });
        });
      });
    });

    suite('when disabled', () => {
      suiteSetup(async () => {
        loadTimeData.overrideValues({isRevampWayfindingEnabled: false});
        Router.getInstance().navigateTo(routes.BASIC);
        mainPageContainer = init();
      });

      suiteTeardown(() => {
        mainPageContainer.remove();
        CrSettingsPrefs.resetForTesting();
        Router.getInstance().resetRouteForTesting();
      });

      test('advanced toggle should render', () => {
        const advancedToggle =
            mainPageContainer.shadowRoot.querySelector('#advancedToggle');
        assertTrue(!!advancedToggle);
      });
    });
  });
});
