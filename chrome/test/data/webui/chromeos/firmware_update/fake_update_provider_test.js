// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeFirmwareUpdates} from 'chrome://accessory-update/fake_data.js';
import {FakeUpdateProvider} from 'chrome://accessory-update/fake_update_provider.js';
import {UpdateObserverRemote} from 'chrome://accessory-update/firmware_update_types.js';

import {assertDeepEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

export function fakeUpdateProviderTest() {
  /** @type {?FakeUpdateProvider} */
  let provider = null;

  setup(() => provider = new FakeUpdateProvider());

  teardown(() => {
    provider.reset();
    provider = null;
  });

  test('ObservePeripheralUpdates', () => {
    provider.setFakeFirmwareUpdates(fakeFirmwareUpdates);

    const updateObserverRemote = /** @type {!UpdateObserverRemote} */ ({
      onUpdateListChanged: (firmwareUpdates) => {
        assertDeepEquals(fakeFirmwareUpdates[0], firmwareUpdates);
      },
    });

    provider.observePeripheralUpdates(updateObserverRemote);
    return provider.getObservePeripheralUpdatesPromiseForTesting();
  });
}
