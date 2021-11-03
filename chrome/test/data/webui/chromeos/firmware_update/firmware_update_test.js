// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeUpdateProvider} from 'chrome://accessory-update/fake_update_provider.js';
import {FirmwareUpdateAppElement} from 'chrome://accessory-update/firmware_update_app.js';
import {UpdateProviderInterface} from 'chrome://accessory-update/firmware_update_types.js';
import {getUpdateProvider, setUpdateProviderForTesting} from 'chrome://accessory-update/mojo_interface_provider.js';

import {assertEquals} from '../../chai_assert.js';

export function firmwareUpdateAppTest() {
  /** @type {?FirmwareUpdateAppElement} */
  let page = null;

  setup(() => {
    page = /** @type {!FirmwareUpdateAppElement} */ (
        document.createElement('firmware-update-app'));
    document.body.appendChild(page);
  });

  teardown(() => {
    page.remove();
    page = null;
  });

  test('LandingPageLoaded', () => {
    // TODO(michaelcheco): Remove this stub test once the page has more
    // capabilities to test.
    assertEquals(
        'Firmware updates',
        page.shadowRoot.querySelector('#header').textContent.trim());
  });

  test('SettingGettingTestProvider', () => {
    let fake_provider =
        /** @type {!UpdateProviderInterface} */ (new FakeUpdateProvider());
    setUpdateProviderForTesting(fake_provider);
    assertEquals(fake_provider, getUpdateProvider());
  });
}
