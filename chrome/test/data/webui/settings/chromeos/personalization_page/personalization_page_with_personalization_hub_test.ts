// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {PersonalizationHubBrowserProxyImpl, SettingsPersonalizationPageElement} from 'chrome://os-settings/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {disableAnimationsAndTransitions} from 'chrome://webui-test/test_api.js';

import {TestPersonalizationHubBrowserProxy} from './test_personalization_hub_browser_proxy.js';

let personalizationPage: SettingsPersonalizationPageElement;
let personalizationHubBrowserProxy: TestPersonalizationHubBrowserProxy;

suite('<settings-personalization-page>', () => {
  suiteSetup(() => {
    disableAnimationsAndTransitions();
    personalizationHubBrowserProxy = new TestPersonalizationHubBrowserProxy();
    PersonalizationHubBrowserProxyImpl.setInstanceForTesting(
        personalizationHubBrowserProxy);
  });

  setup(async () => {
    personalizationPage =
        document.createElement('settings-personalization-page');
    document.body.appendChild(personalizationPage);
    flush();
    await waitAfterNextRender(personalizationPage);
  });

  teardown(() => {
    personalizationPage.remove();
    personalizationHubBrowserProxy.reset();
  });

  test('Personalization hub feature shows only link to hub', () => {
    const crLinks =
        personalizationPage.shadowRoot!.querySelectorAll('cr-link-row');

    assertEquals(1, crLinks.length);
    assertEquals('personalizationHubButton', crLinks[0]!.id);
  });

  test('Opens personalization hub when clicked', async () => {
    const hubLink =
        personalizationPage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#personalizationHubButton');
    assertTrue(!!hubLink);
    hubLink.click();

    await personalizationHubBrowserProxy.whenCalled('openPersonalizationHub');
  });
});
