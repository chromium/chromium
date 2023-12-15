// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {CrButtonElement, CrLinkRowElement, ParentalControlsBrowserProxyImpl, ParentalControlsSettingsCardElement, Router, routes, settingMojom} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {TestParentalControlsBrowserProxy} from './test_parental_controls_browser_proxy.js';

suite('<parental-controls-settings-card>', () => {
  let parentalControlsSettingsCard: ParentalControlsSettingsCardElement;
  let parentalControlsBrowserProxy: TestParentalControlsBrowserProxy;

  suiteSetup(() => {
    loadTimeData.overrideValues({
      // Simulate parental controls.
      showParentalControls: true,
      isChild: false,
    });
  });

  setup(() => {
    parentalControlsBrowserProxy = new TestParentalControlsBrowserProxy();
    ParentalControlsBrowserProxyImpl.setInstanceForTesting(
        parentalControlsBrowserProxy);

    parentalControlsSettingsCard =
        document.createElement('parental-controls-settings-card');
    document.body.appendChild(parentalControlsSettingsCard);
    flush();
  });

  teardown(() => {
    parentalControlsSettingsCard.remove();
  });

  test('parental controls page enabled when online', () => {
    // Setup button is shown and enabled.
    const setupButton =
        parentalControlsSettingsCard.shadowRoot!.querySelector<CrButtonElement>(
            '#parentalControlsItem cr-button');
    assert(setupButton);

    setupButton.click();

    // Ensure that the request to launch the add supervision flow went
    // through.
    assertEquals(
        1,
        parentalControlsBrowserProxy.getCallCount('showAddSupervisionDialog'));
  });

  test('parental controls page disabled when offline', () => {
    // Simulate going offline
    window.dispatchEvent(new CustomEvent('offline'));
    // Setup button is shown but disabled.
    const setupButton =
        parentalControlsSettingsCard.shadowRoot!.querySelector<CrButtonElement>(
            '#parentalControlsItem cr-button');
    assert(setupButton);
    assertTrue(setupButton.disabled);

    setupButton.click();

    // Ensure that the request to launch the add supervision flow does not
    // go through.
    assertEquals(
        0,
        parentalControlsBrowserProxy.getCallCount('showAddSupervisionDialog'));
  });

  test('parental controls page re-enabled when it comes back online', () => {
    // Simulate going offline
    window.dispatchEvent(new CustomEvent('offline'));
    // Setup button is shown but disabled.
    const setupButton =
        parentalControlsSettingsCard.shadowRoot!.querySelector<CrButtonElement>(
            '#parentalControlsItem cr-button');
    assert(setupButton);
    assertTrue(setupButton.disabled);

    // Come back online.
    window.dispatchEvent(new CustomEvent('online'));
    // Setup button is shown and re-enabled.
    assertFalse(setupButton.disabled);
  });

  test('Deep link to parental controls setup button', async () => {
    const params = new URLSearchParams();
    const settingId = settingMojom.Setting.kSetUpParentalControls.toString();
    params.append('settingId', settingId);
    Router.getInstance().navigateTo(routes.OS_PEOPLE, params);
    flush();

    const deepLinkElement =
        parentalControlsSettingsCard.shadowRoot!.querySelector<HTMLElement>(
            '#setupButton');
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, parentalControlsSettingsCard.shadowRoot!.activeElement,
        `Setup button should be focused for settingId=${settingId}.`);
  });

  suite('Chrome OS parental controls page child account tests', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        // Simulate child account.
        isChild: true,
      });
    });

    test('parental controls page child view shown to child account', () => {
      // Get the link row.
      const linkRow = parentalControlsSettingsCard.shadowRoot!
                          .querySelector<CrLinkRowElement>(
                              '#parentalControlsItem cr-link-row');
      assert(linkRow);

      linkRow.click();
      // Ensure that the request to launch FLH went through.
      assertEquals(
          parentalControlsBrowserProxy.getCallCount('launchFamilyLinkSettings'),
          1);
    });
  });
});
