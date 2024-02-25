// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Suite of tests for page availability in the CrOS Settings UI.
 *
 * - This suite is run with the OsSettingsRevampWayfinding feature flag both
 *   enabled and disabled.
 * - This suite is separated into a dedicated file to mitigate test timeouts
 *   since the element is very large.
 */

import 'chrome://os-settings/os_settings.js';

import {AccountManagerBrowserProxyImpl} from 'chrome://os-settings/lazy_load.js';
import {createRouterForTesting, CrSettingsPrefs, MainPageContainerElement, OsSettingsMainElement, OsSettingsUiElement, PageDisplayerElement, Router, routesMojom, SettingsIdleLoadElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestAccountManagerBrowserProxy} from '../os_people_page/test_account_manager_browser_proxy.js';

import {SECTION_EXPECTATIONS, SectionName} from './page_availability_test_helpers.js';

const {Section} = routesMojom;

suite('<os-settings-ui> page availability', () => {
  const isRevampEnabled = loadTimeData.getBoolean('isRevampWayfindingEnabled');
  let ui: OsSettingsUiElement;
  let settingsMain: OsSettingsMainElement;
  let mainPageContainer: MainPageContainerElement;
  let browserProxy: TestAccountManagerBrowserProxy;

  suiteSetup(() => {
    browserProxy = new TestAccountManagerBrowserProxy();
    AccountManagerBrowserProxyImpl.setInstanceForTesting(browserProxy);
  });

  async function createUi() {
    ui = document.createElement('os-settings-ui');
    document.body.appendChild(ui);
    flush();

    await CrSettingsPrefs.initialized;
    const mainElement = ui.shadowRoot!.querySelector('os-settings-main');
    assert(mainElement);
    settingsMain = mainElement;

    const pageElement =
        settingsMain.shadowRoot!.querySelector('main-page-container');
    assert(pageElement);
    mainPageContainer = pageElement;

    // Force load advanced page container
    const advancedPageTemplate =
        mainPageContainer.shadowRoot!.querySelector<SettingsIdleLoadElement>(
            '#advancedPageTemplate');
    assert(advancedPageTemplate);
    await advancedPageTemplate.get();
    flush();
  }

  function getPageDisplayerForSection(section: routesMojom.Section):
      PageDisplayerElement|null {
    return mainPageContainer.shadowRoot!.querySelector<PageDisplayerElement>(
        `page-displayer[section="${section}"]`);
  }

  function assertPageIsStamped(sectionName: SectionName) {
    const pageDisplayer = getPageDisplayerForSection(Section[sectionName]);
    assertTrue(!!pageDisplayer, `${sectionName} page should be stamped.`);
  }

  function assertPageIsNotStamped(sectionName: SectionName) {
    const pageDisplayer = getPageDisplayerForSection(Section[sectionName]);
    assertNull(pageDisplayer, `${sectionName} page should not be stamped.`);
  }

  suite('For normal user', () => {
    suiteSetup(async () => {
      loadTimeData.overrideValues({
        isGuest: false,           // Default to normal user
        isKerberosEnabled: true,  // Simulate kerberos enabled
        allowPowerwash: true,     // Simulate powerwash allowed
      });

      // Reinitialize Router and routes based on load time data
      const testRouter = createRouterForTesting();
      Router.resetInstanceForTesting(testRouter);

      await createUi();

      await browserProxy.whenCalled('getAccounts');
      flush();
    });

    suiteTeardown(() => {
      ui.remove();
    });

    for (const {
           name,
           availableBeforeRevamp,
           availableAfterRevamp,
         } of SECTION_EXPECTATIONS) {
      test(`${name} page availability`, async () => {
        const shouldExpectStamped = (isRevampEnabled && availableAfterRevamp) ||
            (!isRevampEnabled && availableBeforeRevamp);

        if (shouldExpectStamped) {
          assertPageIsStamped(name);
        } else {
          assertPageIsNotStamped(name);
        }
      });
    }
  });

  suite('For guest user', () => {
    suiteSetup(async () => {
      loadTimeData.overrideValues({
        isGuest: true,            // Simulate guest mode
        isKerberosEnabled: true,  // Simulate kerberos enabled
        allowPowerwash: false,    // Powerwash is never enabled in guest mode
      });

      // Reinitialize Router and routes based on load time data
      const testRouter = createRouterForTesting();
      Router.resetInstanceForTesting(testRouter);

      await createUi();

      await browserProxy.whenCalled('getAccounts');
      flush();
    });

    suiteTeardown(() => {
      ui.remove();
    });

    for (const {
           name,
           availableBeforeRevamp,
           availableAfterRevamp,
           availableForGuest,
         } of SECTION_EXPECTATIONS) {
      test(`${name} page availability`, async () => {
        const shouldExpectStamped = availableForGuest &&
            ((isRevampEnabled && availableAfterRevamp) ||
             (!isRevampEnabled && availableBeforeRevamp));

        if (shouldExpectStamped) {
          assertPageIsStamped(name);
        } else {
          assertPageIsNotStamped(name);
        }
      });
    }
  });
});
