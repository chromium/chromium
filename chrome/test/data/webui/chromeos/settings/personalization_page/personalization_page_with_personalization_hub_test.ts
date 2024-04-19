// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {PersonalizationHubBrowserProxyImpl, Router, routes, settingMojom, SettingsPersonalizationPageElement} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestPersonalizationHubBrowserProxy} from './test_personalization_hub_browser_proxy.js';

suite('<settings-personalization-page>', () => {
  let personalizationPage: SettingsPersonalizationPageElement;
  let personalizationHubBrowserProxy: TestPersonalizationHubBrowserProxy;
  const shouldShowMultitaskingInPersonalization = loadTimeData.getBoolean('shouldShowMultitaskingInPersonalization');

  async function createPersonalizationPage(): Promise<void> {
    personalizationPage =
        document.createElement('settings-personalization-page');
    document.body.appendChild(personalizationPage);
    await flushTasks();
  }

  async function deepLinkToSetting(setting: settingMojom.Setting):
      Promise<void> {
    const settingId = setting.toString();
    const params = new URLSearchParams();
    params.append('settingId', settingId);
    Router.getInstance().navigateTo(routes.PERSONALIZATION, params);
    await flushTasks();
  }

  async function assertElementIsDeepLinked(element: HTMLElement):
      Promise<void> {
    assertTrue(isVisible(element));
    await waitAfterNextRender(element);
    assertEquals(element, personalizationPage.shadowRoot!.activeElement);
  }

  suiteSetup(() => {
    personalizationHubBrowserProxy = new TestPersonalizationHubBrowserProxy();
    PersonalizationHubBrowserProxyImpl.setInstanceForTesting(
        personalizationHubBrowserProxy);
  });

  teardown(() => {
    personalizationPage.remove();
    personalizationHubBrowserProxy.reset();
  });

  test('Personalization hub feature shows only link to hub', async () => {
    await createPersonalizationPage();
    const crLinks =
        personalizationPage.shadowRoot!.querySelectorAll('cr-link-row');

    assertEquals(1, crLinks.length);
    assertEquals('personalizationHubButton', crLinks[0]!.id);
  });

  test('Opens personalization hub when clicked', async () => {
    await createPersonalizationPage();
    const hubLink =
        personalizationPage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#personalizationHubButton');
    assertTrue(!!hubLink);
    hubLink.click();

    await personalizationHubBrowserProxy.whenCalled('openPersonalizationHub');
  });

  if (shouldShowMultitaskingInPersonalization) {
    test(
        'Multitasking settings subsection is visible with feature enabled',
        async () => {
          await createPersonalizationPage();
          const multitaskingSettingsSubsection =
              personalizationPage.shadowRoot!.querySelector<HTMLButtonElement>(
                  '#snapWindowSuggestionsSubsection');
          assertTrue(
              isVisible(multitaskingSettingsSubsection),
              'Multitasking settings subsection should be visible.');
        });

    test('Multitasking settings subsection is deep-linkable', async () => {
      await createPersonalizationPage();
      await deepLinkToSetting(settingMojom.Setting.kSnapWindowSuggestions);

      const multitaskingSettingsSubsection =
        personalizationPage.shadowRoot!.querySelector<HTMLButtonElement>(
          '#snapWindowSuggestionsSubsection');
      assertTrue(!!multitaskingSettingsSubsection);
      await assertElementIsDeepLinked(multitaskingSettingsSubsection);
    });
  } else {
    test(
      'Multitasking settings subsection is not visible with feature disabled',
      async () => {
        await createPersonalizationPage();

        const multitaskingSettingsSubsection =
          personalizationPage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#snapWindowSuggestionsSubsection');
        assertFalse(
          isVisible(multitaskingSettingsSubsection),
          'Multitasking settings subsection should not be visible.');
      });
  }
});
