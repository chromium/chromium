// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeInputDeviceSettingsProvider, fakeKeyboards, fakeMice, fakePointingSticks, fakeTouchpads} from 'chrome://os-settings/chromeos/os_settings.js';
import {assertDeepEquals} from 'chrome://webui-test/chai_assert.js';

suite('FakeInputDeviceSettings', function() {
  /**
   * @type {?FakeInputDeviceSettingsProvider}
   */
  let provider = null;

  setup(() => {
    provider = new FakeInputDeviceSettingsProvider();
  });

  teardown(() => {
    provider = null;
  });

  test('setFakeKeyboards', () => {
    provider.setFakeKeyboards(fakeKeyboards);
    return provider.getConnectedKeyboardSettings().then(
        result => assertDeepEquals(fakeKeyboards, result));
  });

  test('setFakeTouchpads', () => {
    provider.setFakeTouchpads(fakeTouchpads);
    return provider.getConnectedTouchpadSettings().then(
        result => assertDeepEquals(fakeTouchpads, result));
  });

  test('setFakeMice', () => {
    provider.setFakeMice(fakeMice);
    return provider.getConnectedMouseSettings().then(
        result => assertDeepEquals(fakeMice, result));
  });

  test('setFakePointingSticks', () => {
    provider.setFakePointingSticks(fakePointingSticks);
    return provider.getConnectedPointingStickSettings().then(
        result => assertDeepEquals(fakePointingSticks, result));
  });

  test('setKeyboardSettings', () => {
    provider.setFakeKeyboards(fakeKeyboards);
    // Update the first keyboard settings with the second keyboard settings.
    const updatedFirstKeyboard = {
      ...fakeKeyboards[0],
      settings: {...fakeKeyboards[1].settings},
    };
    provider.setKeyboardSettings(
        updatedFirstKeyboard.id, updatedFirstKeyboard.settings);
    // Verify if the first keyboard settings are updated.
    return provider.getConnectedKeyboardSettings().then(
        result => assertDeepEquals(updatedFirstKeyboard, result[0]));
  });

  test('setMouseSettings', () => {
    provider.setFakeMice(fakeMice);
    // Update the first mouse settings with the second mouse settings.
    const updatedFirstMouse = {
      ...fakeMice[0],
      settings: {...fakeMice[1].settings},
    };
    provider.setMouseSettings(updatedFirstMouse.id, updatedFirstMouse.settings);
    // Verify if the first mouse settings are updated.
    return provider.getConnectedMouseSettings().then(
        result => assertDeepEquals(updatedFirstMouse, result[0]));
  });

  test('setTouchpadSettings', () => {
    provider.setFakeTouchpads(fakeTouchpads);
    // Update the first touchpad settings with the second touchpad settings.
    const updatedFirstTouchpad = {
      ...fakeTouchpads[0],
      settings: {...fakeTouchpads[1].settings},
    };
    provider.setTouchpadSettings(
        updatedFirstTouchpad.id, updatedFirstTouchpad.settings);
    // Verify if the first touchpad settings are updated.
    return provider.getConnectedTouchpadSettings().then(
        result => assertDeepEquals(updatedFirstTouchpad, result[0]));
  });

  test('setPointingStickSettings', () => {
    provider.setFakePointingSticks(fakePointingSticks);
    // Update the first point stick settings with the second point stick
    // settings.
    const updatedFirstPointingStick = {
      ...fakePointingSticks[0],
      settings: {...fakePointingSticks[1].settings},
    };
    provider.setPointingStickSettings(
        updatedFirstPointingStick.id, updatedFirstPointingStick.settings);
    // Verify if the first point stick settings are updated.
    return provider.getConnectedPointingStickSettings().then(
        result => assertDeepEquals(updatedFirstPointingStick, result[0]));
  });
});
