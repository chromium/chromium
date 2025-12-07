// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import type {AccountManagerBrowserProxy} from 'chrome://os-settings/lazy_load.js';
import {AccountManagerBrowserProxyImpl} from 'chrome://os-settings/lazy_load.js';
import type {OsSettingsPeoplePageElement, ProfileInfoBrowserProxy} from 'chrome://os-settings/os_settings.js';
import {ProfileInfoBrowserProxyImpl, Router, routes, setGraduationHandlerProviderForTesting} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestAccountManagerBrowserProxy} from './test_account_manager_browser_proxy.js';
import {TestGraduationHandler} from './test_graduation_handler_provider.js';
import {TestProfileInfoBrowserProxy} from './test_profile_info_browser_proxy.js';

suite('<os-settings-people-page>', () => {
  const isGraduationFlagEnabled =
      loadTimeData.getBoolean('isGraduationFlagEnabled');
  let peoplePage: OsSettingsPeoplePageElement;
  let browserProxy: ProfileInfoBrowserProxy&TestProfileInfoBrowserProxy;
  let accountManagerBrowserProxy: AccountManagerBrowserProxy&
      TestAccountManagerBrowserProxy;

  function createPage(): void {
    peoplePage = document.createElement('os-settings-people-page');
    document.body.appendChild(peoplePage);
    flush();
  }

  setup(() => {
    browserProxy = new TestProfileInfoBrowserProxy();
    ProfileInfoBrowserProxyImpl.setInstance(browserProxy);

    accountManagerBrowserProxy = new TestAccountManagerBrowserProxy();
    AccountManagerBrowserProxyImpl.setInstanceForTesting(
        accountManagerBrowserProxy);

    Router.getInstance().navigateTo(routes.OS_PEOPLE);
  });

  teardown(() => {
    peoplePage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('account manager settings card is visible', async () => {
    createPage();

    await accountManagerBrowserProxy.whenCalled('getAccounts');
    flush();

    const accountManagerSettingsCard =
        peoplePage.shadowRoot!.querySelector('account-manager-settings-card');

    assertTrue(isVisible(accountManagerSettingsCard));
  });

  test('additional accounts settings card is visible', async () => {
    createPage();

    await accountManagerBrowserProxy.whenCalled('getAccounts');
    flush();

    const additionalAccountsSettingsCard = peoplePage.shadowRoot!.querySelector(
        'additional-accounts-settings-card');

    assertTrue(isVisible(additionalAccountsSettingsCard));
  });

  test(
      'parental controls settings card is visible when showParentalControls is enabled',
      async () => {
        loadTimeData.overrideValues({
          // Simulate parental controls.
          showParentalControls: true,
        });

        createPage();

        await accountManagerBrowserProxy.whenCalled('getAccounts');
        flush();

        const parentalControlsSettingsCard =
            peoplePage.shadowRoot!.querySelector(
                'parental-controls-settings-card');

        assertTrue(isVisible(parentalControlsSettingsCard));
      });

  test(
      'parental controls settings card is not shown when showParentalControls is disabled',
      async () => {
        loadTimeData.overrideValues({
          // Simulate parental controls.
          showParentalControls: false,
        });

        createPage();

        await accountManagerBrowserProxy.whenCalled('getAccounts');
        flush();

        const parentalControlsSettingsCard =
            peoplePage.shadowRoot!.querySelector(
                'parental-controls-settings-card');

        assertNull(parentalControlsSettingsCard);
      });

  if (isGraduationFlagEnabled) {
    test('Graduation settings card is shown when app is enabled', async () => {
      loadTimeData.overrideValues({
        isGraduationAppEnabled: true,
      });
      const handler = new TestGraduationHandler();
      setGraduationHandlerProviderForTesting(handler);

      createPage();
      await accountManagerBrowserProxy.whenCalled('getAccounts');
      flush();

      const graduationSettingsCard =
          peoplePage.shadowRoot!.querySelector('graduation-settings-card');
      assertTrue(isVisible(graduationSettingsCard));

      // Simulate pref change to disable app.
      const observer = handler.getObserverRemote();
      assertTrue(!!observer);
      observer.onGraduationAppUpdated(false);
      await waitAfterNextRender(peoplePage);

      assertFalse(isVisible(
          peoplePage.shadowRoot!.querySelector('graduation-settings-card')));
    });

    test(
        'Graduation settings card is not shown when app is disabled',
        async () => {
          loadTimeData.overrideValues({
            isGraduationAppEnabled: false,
          });
          const handler = new TestGraduationHandler();
          setGraduationHandlerProviderForTesting(handler);

          createPage();
          await accountManagerBrowserProxy.whenCalled('getAccounts');
          flush();

          const graduationSettingsCard =
              peoplePage.shadowRoot!.querySelector('graduation-settings-card');
          assertNull(graduationSettingsCard);

          // Simulate pref change to enable app.
          const observer = handler.getObserverRemote();
          assertTrue(!!observer);
          observer.onGraduationAppUpdated(true);
          await waitAfterNextRender(peoplePage);

          assertTrue(isVisible(peoplePage.shadowRoot!.querySelector(
              'graduation-settings-card')));
        });
  } else {
    test(
        'Graduation settings card is not shown when feature is disabled',
        async () => {
          createPage();
          await accountManagerBrowserProxy.whenCalled('getAccounts');
          flush();

          const graduationSettingsCard =
              peoplePage.shadowRoot!.querySelector('graduation-settings-card');
          assertNull(graduationSettingsCard);
        });
  }
});
