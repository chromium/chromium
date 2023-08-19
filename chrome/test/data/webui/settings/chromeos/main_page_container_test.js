// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {createPageAvailabilityForTesting, createRouterForTesting, CrSettingsPrefs, Router, routes, routesMojom, setContactManagerForTesting, setNearbyShareSettingsForTesting} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeContactManager} from 'chrome://webui-test/nearby_share/shared/fake_nearby_contact_manager.js';
import {FakeNearbyShareSettings} from 'chrome://webui-test/nearby_share/shared/fake_nearby_share_settings.js';

const {Section} = routesMojom;

suite('<main-page-container>', () => {
  /** @type {?MainPageContainerElement} */
  let mainPageContainer = null;

  /** @type {?SettingsPrefsElement} */
  let prefElement = null;

  /** @type {!FakeContactManager} */
  let fakeContactManager = null;
  /** @type {!FakeNearbyShareSettings} */
  let fakeNearbyShareSettings = null;

  suiteSetup(async () => {
    loadTimeData.overrideValues({isKerberosEnabled: true});

    fakeContactManager = new FakeContactManager();
    setContactManagerForTesting(fakeContactManager);
    fakeContactManager.setupContactRecords();

    fakeNearbyShareSettings = new FakeNearbyShareSettings();
    setNearbyShareSettingsForTesting(fakeNearbyShareSettings);

    PolymerTest.clearBody();
    prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);
    await CrSettingsPrefs.initialized;
  });

  /** @return {MainPageContainerElement} */
  function init() {
    // Reinitialize Router and routes based on load time data.
    const testRouter = createRouterForTesting();
    Router.resetInstanceForTesting(testRouter);

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

        // About page
        {
          pageName: 'kAboutChromeOs',
          elementName: 'os-about-page',
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
      suiteSetup(() => {
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

      test('advanced toggle should not render', () => {
        const advancedToggle =
            mainPageContainer.shadowRoot.querySelector('#advancedToggle');
        assertNull(advancedToggle);
      });
    });

    suite('when disabled', () => {
      suiteSetup(() => {
        // Simulate feature flag disabled
        loadTimeData.overrideValues({isRevampWayfindingEnabled: false});
        document.body.classList.remove('revamp-wayfinding-enabled');

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
