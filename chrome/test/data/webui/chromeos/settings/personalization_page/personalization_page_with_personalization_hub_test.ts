// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import type {SettingsPersonalizationPageElement} from 'chrome://os-settings/os_settings.js';
import {PersonalizationHubBrowserProxyImpl} from 'chrome://os-settings/os_settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestPersonalizationHubBrowserProxy} from './test_personalization_hub_browser_proxy.js';

suite('<settings-personalization-page>', () => {
  let personalizationPage: SettingsPersonalizationPageElement;
  let personalizationHubBrowserProxy: TestPersonalizationHubBrowserProxy;

  async function createPersonalizationPage(): Promise<void> {
    personalizationPage =
        document.createElement('settings-personalization-page');
    document.body.appendChild(personalizationPage);
    await flushTasks();
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
});
