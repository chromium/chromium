// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://nearby/strings.m.js';
// #import 'chrome://nearby/shared/nearby_onboarding_page.m.js';
// #import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// #import {setContactManagerForTesting} from 'chrome://nearby/shared/nearby_contact_manager.m.js';
// #import {setNearbyShareSettingsForTesting} from 'chrome://nearby/shared/nearby_share_settings.m.js';
// #import {FakeNearbyShareSettings} from './fake_nearby_share_settings.m.js';
// #import {assertEquals, assertTrue, assertFalse} from '../../chai_assert.js';
// #import {waitAfterNextRender} from '../../test_util.m.js';
// clang-format on

suite('nearby-onboarding-page', function() {
  /** @type {!NearbyOnboardingPageElement} */
  let element;
  /** @type {!string} */
  const deviceName = 'Test\'s Device';
  /** @type {!nearby_share.FakeNearbyShareSettings} */
  let fakeSettings;

  setup(function() {
    fakeSettings = new nearby_share.FakeNearbyShareSettings();
    fakeSettings.setEnabled(true);
    nearby_share.setNearbyShareSettingsForTesting(fakeSettings);

    document.body.innerHTML = '';

    element = /** @type {!NearbyOnboardingPageElement} */ (
        document.createElement('nearby-onboarding-page'));
    element.settings = {
      enabled: false,
      deviceName: deviceName,
      dataUsage: nearbyShare.mojom.DataUsage.kOnline,
      visibility: nearbyShare.mojom.Visibility.kAllContacts,
      isOnboardingComplete: false,
      allowedContacts: [],
    };
    document.body.appendChild(element);
    element.fire('view-enter-start');
  });

  test('Renders onboarding page', async function() {
    assertEquals('NEARBY-ONBOARDING-PAGE', element.tagName);
    // Verify the device name is shown correctly.
    assertEquals(deviceName, element.$$('#deviceName').value);
  });

  test('Device name is focused', async () => {
    const input = /** @type {!CrInputElement} */ (element.$$('#deviceName'));
    await test_util.waitAfterNextRender(/** @type {!HTMLElement} */ (input));
    assertEquals(input, element.shadowRoot.activeElement);
  });

  test('validate device name preference', async () => {
    loadTimeData.overrideValues({
      'nearbyShareDeviceNameEmptyError': 'non-empty',
      'nearbyShareDeviceNameTooLongError': 'non-empty',
      'nearbyShareDeviceNameInvalidCharactersError': 'non-empty'
    });

    const input = /** @type {!CrInputElement} */ (element.$$('#deviceName'));
    const pageTemplate = element.$$('nearby-page-template');

    fakeSettings.setNextDeviceNameResult(
        nearbyShare.mojom.DeviceNameValidationResult.kErrorEmpty);
    input.fire('input');
    // Allow the validation promise to resolve.
    await test_util.waitAfterNextRender(/** @type {!HTMLElement} */ (input));
    assertTrue(input.invalid);
    assertTrue(pageTemplate.actionDisabled);

    fakeSettings.setNextDeviceNameResult(
        nearbyShare.mojom.DeviceNameValidationResult.kValid);
    input.fire('input');
    await test_util.waitAfterNextRender(/** @type {!HTMLElement} */ (input));
    assertFalse(input.invalid);
    assertFalse(pageTemplate.actionDisabled);
  });
});
