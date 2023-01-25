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
});
