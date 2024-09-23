// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {AccountManagerBrowserProxyImpl} from 'chrome://os-settings/lazy_load.js';
import {createPageAvailabilityForTesting, createRouterForTesting, CrSettingsPrefs, MainPageContainerElement, Router, routes, routesMojom, setContactManagerForTesting, setNearbyShareSettingsForTesting, SettingsPrefsElement} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeContactManager} from 'chrome://webui-test/chromeos/nearby_share/shared/fake_nearby_contact_manager.js';
import {FakeNearbyShareSettings} from 'chrome://webui-test/chromeos/nearby_share/shared/fake_nearby_share_settings.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestAccountManagerBrowserProxy} from '../os_people_page/test_account_manager_browser_proxy.js';

const {Section} = routesMojom;

suite('<main-page-container>', () => {
  const isRevampWayfindingEnabled =
      loadTimeData.getBoolean('isRevampWayfindingEnabled');

  let mainPageContainer: MainPageContainerElement;
  let prefElement: SettingsPrefsElement;
  let fakeContactManager: FakeContactManager;
  let fakeNearbyShareSettings: FakeNearbyShareSettings;
  let browserProxy: TestAccountManagerBrowserProxy;

  suiteSetup(async () => {
    loadTimeData.overrideValues({isKerberosEnabled: true});

    fakeContactManager = new FakeContactManager();
    setContactManagerForTesting(fakeContactManager);
    fakeContactManager.setupContactRecords();

    fakeNearbyShareSettings = new FakeNearbyShareSettings();
    setNearbyShareSettingsForTesting(fakeNearbyShareSettings);

    browserProxy = new TestAccountManagerBrowserProxy();
    AccountManagerBrowserProxyImpl.setInstanceForTesting(browserProxy);

    prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);
    await CrSettingsPrefs.initialized;
  });

  function init(): MainPageContainerElement {
    // Reinitialize Router and routes based on load time data.
    const testRouter = createRouterForTesting();
    Router.resetInstanceForTesting(testRouter);

    const element = document.createElement('main-page-container');
    element.prefs = prefElement.prefs!;
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
          mainPageContainer.shadowRoot!.querySelector('settings-idle-load');
      assertTrue(!!idleRender);
      await idleRender.get();
    });

    suiteTeardown(() => {
      mainPageContainer.remove();
      CrSettingsPrefs.resetForTesting();
      Router.getInstance().resetRouteForTesting();
    });

    interface Pages {
      pageName: keyof typeof Section;
      elementName: string;
    }

    let pages: Pages[];
    if (isRevampWayfindingEnabled) {
      pages = [
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
          pageName: 'kPrivacyAndSecurity',
          elementName: 'os-settings-privacy-page',
        },
        {
          pageName: 'kApps',
          elementName: 'os-settings-apps-page',
        },
        {
          pageName: 'kSystemPreferences',
          elementName: 'settings-system-preferences-page',
        },
        {
          pageName: 'kAccessibility',
          elementName: 'os-settings-a11y-page',
        },
        {
          pageName: 'kAboutChromeOs',
          elementName: 'os-about-page',
        },
      ];
    } else {
      pages = [
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
      ];
    }

    pages.forEach(({pageName, elementName}) => {
      test(`${String(pageName)} page is controlled by pageAvailability`, () => {
        // Make page available
        mainPageContainer.pageAvailability = {
          ...mainPageContainer.pageAvailability,
          [Section[pageName]]: true,
        };
        flush();

        let pageElement =
            mainPageContainer.shadowRoot!.querySelector(elementName);
        assertTrue(!!pageElement, `<${elementName}> should exist.`);

        // Make page unavailable
        mainPageContainer.pageAvailability = {
          ...mainPageContainer.pageAvailability,
          [Section[pageName]]: false,
        };
        flush();

        pageElement = mainPageContainer.shadowRoot!.querySelector(elementName);
        assertNull(pageElement, `<${elementName}> should not exist.`);
      });
    });
  });

  suite('Advanced toggle', () => {
    suiteSetup(() => {
      mainPageContainer = init();
    });

    suiteTeardown(() => {
      mainPageContainer.remove();
      CrSettingsPrefs.resetForTesting();
      Router.getInstance().resetRouteForTesting();
    });

    if (isRevampWayfindingEnabled) {
      test('Advanced toggle should not be stamped', () => {
        const advancedToggle =
            mainPageContainer.shadowRoot!.querySelector('#advancedToggle');
        assertNull(advancedToggle);
      });
    } else {
      test('Advanced toggle should be visible', () => {
        const advancedToggle =
            mainPageContainer.shadowRoot!.querySelector('#advancedToggle');
        assertTrue(isVisible(advancedToggle));
      });
    }
  });
});
