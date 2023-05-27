// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {createPageAvailabilityForTesting, CrSettingsPrefs, Router, routes, setContactManagerForTesting, setNearbyShareSettingsForTesting} from 'chrome://os-settings/os_settings.js';
import {setBluetoothConfigForTesting} from 'chrome://resources/ash/common/bluetooth/cros_bluetooth_config.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeBluetoothConfig} from 'chrome://webui-test/cr_components/chromeos/bluetooth/fake_bluetooth_config.js';
import {FakeContactManager} from 'chrome://webui-test/nearby_share/shared/fake_nearby_contact_manager.js';
import {FakeNearbyShareSettings} from 'chrome://webui-test/nearby_share/shared/fake_nearby_share_settings.js';

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

      test('advanced toggle should not render', () => {
        const advancedToggle =
            mainPageContainer.shadowRoot.querySelector('#advancedToggle');
        assertNull(advancedToggle);
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
