// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://nearby/strings.m.js';
import 'chrome://nearby/shared/nearby_onboarding_one_page.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {setNearbyShareSettingsForTesting} from 'chrome://nearby/shared/nearby_share_settings.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {DataUsage, DeviceNameValidationResult, FastInitiationNotificationState, Visibility} from 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

import {FakeNearbyShareSettings} from './fake_nearby_share_settings.js';

suite('nearby-onboarding-one-page', function() {
  /** @type {!NearbyOnboardingOnePageElement} */
  let element;
  /** @type {!string} */
  const deviceName = 'Test\'s Device';
  /** @type {!FakeNearbyShareSettings} */
  let fakeSettings;

  setup(function() {
    fakeSettings = new FakeNearbyShareSettings();
    setNearbyShareSettingsForTesting(fakeSettings);

    document.body.innerHTML = trustedTypes.emptyHTML;

    element = /** @type {!NearbyOnboardingOnePageElement} */ (
        document.createElement('nearby-onboarding-one-page'));
    element.settings = {
      enabled: false,
      fastInitiationNotificationState: FastInitiationNotificationState.kEnabled,
      isFastInitiationHardwareSupported: true,
      deviceName: deviceName,
      dataUsage: DataUsage.kOnline,
      visibility: Visibility.kUnknown,
      isOnboardingComplete: false,
      allowedContacts: [],
    };
    document.body.appendChild(element);
    const viewEnterStartEvent = new CustomEvent('view-enter-start', {
      bubbles: true,
      composed: true,
    });
    element.dispatchEvent(viewEnterStartEvent);
  });

  test('Renders one-page onboarding page', async function() {
    assertEquals('NEARBY-ONBOARDING-ONE-PAGE', element.tagName);
    // Verify the device name is shown correctly.
    assertEquals(
        deviceName, element.shadowRoot.querySelector('#deviceName').value);
  });

  test('Visibility button shows all contacts', async function() {
    const buttonContent =
        element.shadowRoot.querySelector('#visibilityModeLabel')
            .textContent.trim();
    assertEquals('All contacts', buttonContent);
  });

  test('Device name is focused', async () => {
    const input = /** @type {!CrInputElement} */ (
        element.shadowRoot.querySelector('#deviceName'));
    await waitAfterNextRender(/** @type {!HTMLElement} */ (input));
    assertEquals(input, element.shadowRoot.activeElement);
  });

  test('validate device name preference', async () => {
    loadTimeData.overrideValues({
      'nearbyShareDeviceNameEmptyError': 'non-empty',
      'nearbyShareDeviceNameTooLongError': 'non-empty',
      'nearbyShareDeviceNameInvalidCharactersError': 'non-empty',
    });

    const input = /** @type {!CrInputElement} */ (
        element.shadowRoot.querySelector('#deviceName'));
    const pageTemplate = /** @type {!NearbyPageTemplateElement} */ (
        element.shadowRoot.querySelector('nearby-page-template'));

    fakeSettings.setNextDeviceNameResult(
        DeviceNameValidationResult.kErrorEmpty);
    input.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));
    // Allow the validation promise to resolve.
    await waitAfterNextRender(/** @type {!HTMLElement} */ (input));
    assertTrue(input.invalid);
    assertTrue(pageTemplate.actionDisabled);

    fakeSettings.setNextDeviceNameResult(DeviceNameValidationResult.kValid);
    input.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));
    await waitAfterNextRender(/** @type {!HTMLElement} */ (input));
    assertFalse(input.invalid);
    assertFalse(pageTemplate.actionDisabled);
  });
});
