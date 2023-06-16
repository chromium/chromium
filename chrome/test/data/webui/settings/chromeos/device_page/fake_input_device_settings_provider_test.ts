// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeGraphicsTablets, FakeInputDeviceSettingsProvider, fakeKeyboards, fakeMice, fakePointingSticks, fakeStyluses, fakeTouchpads, ModifierKey} from 'chrome://os-settings/os_settings.js';
import {assertDeepEquals} from 'chrome://webui-test/chai_assert.js';

suite('FakeInputDeviceSettings', () => {
  let provider: FakeInputDeviceSettingsProvider;

  setup(() => {
    provider = new FakeInputDeviceSettingsProvider();
  });

  test('setFakeKeyboards', async () => {
    provider.setFakeKeyboards(fakeKeyboards);
    const result = await provider.getConnectedKeyboardSettings();
    assertDeepEquals(fakeKeyboards, result);
  });

  test('setFakeTouchpads', async () => {
    provider.setFakeTouchpads(fakeTouchpads);
    const result = await provider.getConnectedTouchpadSettings();
    assertDeepEquals(fakeTouchpads, result);
  });

  test('setFakeMice', async () => {
    provider.setFakeMice(fakeMice);
    const result = await provider.getConnectedMouseSettings();
    assertDeepEquals(fakeMice, result);
  });

  test('setFakePointingSticks', async () => {
    provider.setFakePointingSticks(fakePointingSticks);
    const result = await provider.getConnectedPointingStickSettings();
    assertDeepEquals(fakePointingSticks, result);
  });

  test('setFakeStyluses', async () => {
    provider.setFakeStyluses(fakeStyluses);
    const result = await provider.getConnectedStylusSettings();
    assertDeepEquals(fakeStyluses, result);
  });

  test('setFakeGraphicsTablets', async () => {
    provider.setFakeGraphicsTablets(fakeGraphicsTablets);
    const result = await provider.getConnectedGraphicsTabletSettings();
    assertDeepEquals(fakeGraphicsTablets, result);
  });

  test('setKeyboardSettings', async () => {
    provider.setFakeKeyboards(fakeKeyboards);
    // Update the first keyboard settings with the second keyboard settings.
    const updatedFirstKeyboard = {
      ...fakeKeyboards[0],
      settings: {...fakeKeyboards[1]!.settings},
    };
    provider.setKeyboardSettings(
        updatedFirstKeyboard.id!, updatedFirstKeyboard.settings);
    // Verify if the first keyboard settings are updated.
    const result = await provider.getConnectedKeyboardSettings();
    assertDeepEquals(updatedFirstKeyboard, result[0]);
  });

  test('setMouseSettings', async () => {
    provider.setFakeMice(fakeMice);
    // Update the first mouse settings with the second mouse settings.
    const updatedFirstMouse = {
      ...fakeMice[0],
      settings: {...fakeMice[1]!.settings},
    };
    provider.setMouseSettings(
        updatedFirstMouse.id!, updatedFirstMouse.settings);
    // Verify if the first mouse settings are updated.
    const result = await provider.getConnectedMouseSettings();
    assertDeepEquals(updatedFirstMouse, result[0]);
  });

  test('setTouchpadSettings', async () => {
    provider.setFakeTouchpads(fakeTouchpads);
    // Update the first touchpad settings with the second touchpad settings.
    const updatedFirstTouchpad = {
      ...fakeTouchpads[0],
      settings: {...fakeTouchpads[1]!.settings},
    };
    provider.setTouchpadSettings(
        updatedFirstTouchpad.id!, updatedFirstTouchpad.settings);
    // Verify if the first touchpad settings are updated.
    const result = await provider.getConnectedTouchpadSettings();
    assertDeepEquals(updatedFirstTouchpad, result[0]);
  });

  test('setPointingStickSettings', async () => {
    provider.setFakePointingSticks(fakePointingSticks);
    // Update the first point stick settings with the second point stick
    // settings.
    const updatedFirstPointingStick = {
      ...fakePointingSticks[0],
      settings: {...fakePointingSticks[1]!.settings},
    };
    provider.setPointingStickSettings(
        updatedFirstPointingStick.id!, updatedFirstPointingStick.settings);
    // Verify if the first point stick settings are updated.
    const result = await provider.getConnectedPointingStickSettings();
    assertDeepEquals(updatedFirstPointingStick, result[0]);
  });

  test('restoreDefaultKeyboardRemappings', async () => {
    provider.setFakeKeyboards(fakeKeyboards);
    // Restore the default remappings for the first keyboard settings.
    provider.restoreDefaultKeyboardRemappings(fakeKeyboards[0]!.id!);
    // Verify if the first keyboard settings are updated.
    const result = await provider.getConnectedKeyboardSettings();
    assertDeepEquals(result[0]!.settings!.modifierRemappings, {
      [ModifierKey.kControl]: ModifierKey.kMeta,
      [ModifierKey.kMeta]: ModifierKey.kControl,
    });
  });
});
