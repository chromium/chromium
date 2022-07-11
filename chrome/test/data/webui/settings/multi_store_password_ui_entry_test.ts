// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MultiStorePasswordUiEntry} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {createPasswordEntry} from './passwords_and_autofill_fake_data.js';

suite('MultiStorePasswordUiEntry', function() {
  test('verifyWhenInBothStores', function() {
    const multiStoreEntry = createPasswordEntry({
      url: 'g.com',
      username: 'user',
      id: 0,
      inAccountStore: true,
      inProfileStore: true
    });

    const multiStoreDeviceEntry =
        new MultiStorePasswordUiEntry(multiStoreEntry);
    assertTrue(multiStoreDeviceEntry.isPresentOnDevice());
    assertTrue(multiStoreDeviceEntry.isPresentInAccount());
    assertEquals(multiStoreDeviceEntry.getAnyId(), 0);
  });

  test('verifyInAccount', function() {
    const accountEntry = createPasswordEntry({
      url: 'g.com',
      username: 'user',
      id: 0,
      inAccountStore: true,
    });

    const multiStoreDeviceEntry = new MultiStorePasswordUiEntry(accountEntry);
    assertFalse(multiStoreDeviceEntry.isPresentOnDevice());
    assertTrue(multiStoreDeviceEntry.isPresentInAccount());
    assertEquals(multiStoreDeviceEntry.getAnyId(), 0);
  });

  test('verifyInProfile', function() {
    const deviceEntry = createPasswordEntry({
      url: 'g.com',
      username: 'user',
      id: 0,
      inProfileStore: true,
    });

    const multiStoreDeviceEntry = new MultiStorePasswordUiEntry(deviceEntry);
    assertTrue(multiStoreDeviceEntry.isPresentOnDevice());
    assertFalse(multiStoreDeviceEntry.isPresentInAccount());
    assertEquals(multiStoreDeviceEntry.getAnyId(), 0);
  });
});
