// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {fakeFirmwareUpdates} from 'chrome://accessory-update/fake_data.js';
import {FakeUpdateProvider} from 'chrome://accessory-update/fake_update_provider.js';
import {UpdateObserverRemote} from 'chrome://accessory-update/firmware_update.mojom-webui.js';
import {assertDeepEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('FakeUpdateProviderTest', () => {
  let provider: FakeUpdateProvider|null = null;

  setup(() => provider = new FakeUpdateProvider());

  teardown(() => {
    provider?.reset();
    provider = null;
  });

  test('ObservePeripheralUpdates', () => {
    assert(provider);
    provider.setFakeFirmwareUpdates(fakeFirmwareUpdates);

    const updateObserverRemote ={
      onUpdateListChanged: (firmwareUpdates) => {
        assertDeepEquals(fakeFirmwareUpdates[0], firmwareUpdates);
      },
    } as UpdateObserverRemote;

    provider.observePeripheralUpdates(updateObserverRemote);
    return provider.getObservePeripheralUpdatesPromiseForTesting();
  });
});
