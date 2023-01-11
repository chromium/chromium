// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeInputDeviceSettingsProvider, MetaKey} from 'chrome://os-settings/chromeos/os_settings.js';
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
    const fakeKeyboards = [{
      id: 0,
      name: 'fake-keyboard',
      isExternal: false,
      metaKey: MetaKey.COMMAND,
      modifierKeys: [],
      settings: {},
    }];

    provider.setFakeKeyboards(fakeKeyboards);
    return provider.getFakeKeyboards().then(
        result => assertDeepEquals(fakeKeyboards, result));
  });

  test('setFakeTouchpads', () => {
    const fakeTouchpads = [{
      id: 1,
      name: 'fake-touchpad',
      isExternal: false,
      isHaptic: false,
    }];

    provider.setFakeTouchpads(fakeTouchpads);
    return provider.getFakeTouchpads().then(
        result => assertDeepEquals(fakeTouchpads, result));
  });

  test('setFakeMice', () => {
    const fakeMice = [{
      id: 2,
      name: 'fake-mouse',
      isExternal: false,
    }];

    provider.setFakeMice(fakeMice);
    return provider.getFakeMice().then(
        result => assertDeepEquals(fakeMice, result));
  });
});
