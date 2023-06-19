// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {createPageAvailabilityForTesting, CrSettingsPrefs, Router, routes, routesMojom, setContactManagerForTesting, setNearbyShareSettingsForTesting} from 'chrome://os-settings/os_settings.js';
import {setBluetoothConfigForTesting} from 'chrome://resources/ash/common/bluetooth/cros_bluetooth_config.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertNotEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeBluetoothConfig} from 'chrome://webui-test/cr_components/chromeos/bluetooth/fake_bluetooth_config.js';
import {FakeContactManager} from 'chrome://webui-test/nearby_share/shared/fake_nearby_contact_manager.js';
import {FakeNearbyShareSettings} from 'chrome://webui-test/nearby_share/shared/fake_nearby_share_settings.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

const {Section} = routesMojom;

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
          pageName: 'kNetwork',
          elementName: 'settings-internet-page',
        },
        {
          pageName: 'kBluetooth',
          elementName: 'os-settings-bluetooth-page',
        },
        {
          pageName: 'kMultiDevice',
          elementName: 'settings-multidevice-page',
        },
        {
          pageName: 'kKerberos',
          elementName: 'settings-kerberos-page',
        },
        {
          pageName: 'kPeople',
          elementName: 'os-settings-people-page',
        },
        {
          pageName: 'kDevice',
          elementName: 'settings-device-page',
        },
        {
          pageName: 'kPersonalization',
          elementName: 'settings-personalization-page',
        },
        {
          pageName: 'kSearchAndAssistant',
          elementName: 'os-settings-search-page',
        },
        {
          pageName: 'kPrivacyAndSecurity',
          elementName: 'os-settings-privacy-page',
        },
        {
          pageName: 'kApps',
          elementName: 'os-settings-apps-page',
        },
        {
          pageName: 'kAccessibility',
          elementName: 'os-settings-a11y-page',
        },

        // Advanced section pages
        {
          pageName: 'kDateAndTime',
          elementName: 'settings-date-time-page',
        },
        {
          pageName: 'kLanguagesAndInput',
          elementName: 'os-settings-languages-section',
        },
        {
          pageName: 'kFiles',
          elementName: 'os-settings-files-page',
        },
        {
          pageName: 'kPrinting',
          elementName: 'os-settings-printing-page',
        },
        {
          pageName: 'kCrostini',
          elementName: 'settings-crostini-page',
        },
        {
          pageName: 'kReset',
          elementName: 'os-settings-reset-page',
        },
    ].forEach(({pageName, elementName}) => {
      test(`${pageName} page is controlled by pageAvailability`, () => {
        // Make page available
        mainPageContainer.pageAvailability = {
          ...mainPageContainer.pageAvailability,
          [Section[pageName]]: true,
        };
        flush();

        let pageElement =
            mainPageContainer.shadowRoot.querySelector(elementName);
        assertTrue(!!pageElement, `<${elementName}> should exist.`);

        // Make page unavailable
        mainPageContainer.pageAvailability = {
          ...mainPageContainer.pageAvailability,
          [Section[pageName]]: false,
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
        // Simulate feature flag enabled
        loadTimeData.overrideValues({isRevampWayfindingEnabled: true});
        document.body.classList.add('revamp-wayfinding-enabled');

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
        const {Section} = routesMojom;

        /**
         * Asserts the following:
         * - Only one page is marked active
         * - Active page does not have style "display: none"
         * - Active page is focused
         * - Inactive pages have style "display: none"
         */
        function assertOnlyActivePageIsVisible(section) {
          const pages =
              mainPageContainer.shadowRoot.querySelectorAll('page-displayer');
          let numActive = 0;

          for (const page of pages) {
            const displayStyle = getComputedStyle(page).display;
            if (page.hasAttribute('active')) {
              numActive++;
              assertNotEquals('none', displayStyle);
              assertEquals(section, page.section);
              assertEquals(page, mainPageContainer.shadowRoot.activeElement);
            } else {
              assertEquals('none', displayStyle);
            }
          }

          assertEquals(1, numActive);
        }

        suite('From Initial', () => {
          test('to Root should only show Network page', () => {
            assertOnlyActivePageIsVisible(Section.kNetwork);
          });
        });

        suite('From Root', () => {
          test('to Page should result in only one active page', async () => {
            // Simulate navigating from root to Network page
            const navigationCompletePromise =
                eventToPromise('show-container', window);
            Router.getInstance().navigateTo(routes.INTERNET);
            await navigationCompletePromise;

            assertOnlyActivePageIsVisible(Section.kNetwork);
          });

          test('to Subpage should result in only one active page', async () => {
            // Simulate navigating from root to Bluetooth subpage
            const navigationCompletePromise =
                eventToPromise('show-container', window);
            Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICES);
            await navigationCompletePromise;

            assertOnlyActivePageIsVisible(Section.kBluetooth);
          });

          test(
              'to Root by clearing search should show Network page',
              async () => {
                Router.getInstance().navigateTo(
                    routes.BASIC, new URLSearchParams('search=bluetooth'));

                const navigationCompletePromise =
                    eventToPromise('show-container', window);
                Router.getInstance().navigateTo(
                    routes.BASIC, /*dynamicParameters=*/ undefined,
                    /*removeSearch=*/ true);
                await navigationCompletePromise;

                assertOnlyActivePageIsVisible(Section.kNetwork);
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

                assertOnlyActivePageIsVisible(Section.kBluetooth);
              });

          test('to Subpage should result in only one active page', async () => {
            // Simulate navigating from A11y page to A11y display subpage
            Router.getInstance().navigateTo(routes.OS_ACCESSIBILITY);
            const navigationCompletePromise =
                eventToPromise('show-container', window);
            Router.getInstance().navigateTo(
                routes.A11Y_DISPLAY_AND_MAGNIFICATION);
            await navigationCompletePromise;

            assertOnlyActivePageIsVisible(Section.kAccessibility);
          });

          test('to Root should result in only one active page', async () => {
            // Simulate navigating from Network page to root
            Router.getInstance().navigateTo(routes.INTERNET);
            const navigationCompletePromise =
                eventToPromise('show-container', window);
            Router.getInstance().navigateTo(routes.BASIC);
            await navigationCompletePromise;

            assertOnlyActivePageIsVisible(Section.kNetwork);
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
