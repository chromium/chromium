// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {AccountManagerBrowserProxyImpl} from 'chrome://os-settings/lazy_load.js';
import type {MainPageContainerElement, SettingsPrefsElement} from 'chrome://os-settings/os_settings.js';
import {createPageAvailabilityForTesting, createRouterForTesting, CrSettingsPrefs, Router, routes, routesMojom, setContactManagerForTesting, setNearbyShareSettingsForTesting} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeContactManager} from 'chrome://webui-test/chromeos/nearby_share/shared/fake_nearby_contact_manager.js';
import {FakeNearbyShareSettings} from 'chrome://webui-test/chromeos/nearby_share/shared/fake_nearby_share_settings.js';

import {TestAccountManagerBrowserProxy} from '../os_people_page/test_account_manager_browser_proxy.js';

const {Section} = routesMojom;

suite('<main-page-container>', () => {
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
    suiteSetup(() => {
      Router.getInstance().navigateTo(routes.BASIC);
      mainPageContainer = init();
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

    const pages: Pages[] = [
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
});
