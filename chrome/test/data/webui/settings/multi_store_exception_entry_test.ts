// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MultiStoreExceptionEntry} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {createExceptionEntry} from './passwords_and_autofill_fake_data.js';

suite('MultiStoreExceptionEntry', function() {
  test('verifyIds', function() {
    const deviceEntry = createExceptionEntry({url: 'g.com', id: 0});
    deviceEntry.fromAccountStore = false;
    const accountEntry = createExceptionEntry({url: 'g.com', id: 1});
    accountEntry.fromAccountStore = true;

    const multiStoreDeviceEntry = new MultiStoreExceptionEntry(deviceEntry);
    assertTrue(multiStoreDeviceEntry.isPresentOnDevice());
    assertFalse(multiStoreDeviceEntry.isPresentInAccount());
    assertEquals(multiStoreDeviceEntry.getAnyId(), 0);

    const multiStoreAccountEntry = new MultiStoreExceptionEntry(accountEntry);
    assertFalse(multiStoreAccountEntry.isPresentOnDevice());
    assertTrue(multiStoreAccountEntry.isPresentInAccount());
    assertEquals(multiStoreAccountEntry.getAnyId(), 1);

    const multiStoreEntryFromBoth = new MultiStoreExceptionEntry(deviceEntry);
    assertTrue(multiStoreEntryFromBoth.mergeInPlace(accountEntry));
    assertTrue(multiStoreEntryFromBoth.isPresentOnDevice());
    assertTrue(multiStoreEntryFromBoth.isPresentInAccount());
    assertTrue(
        multiStoreEntryFromBoth.getAnyId() === 0 ||
        multiStoreEntryFromBoth.getAnyId() === 1);
  });

  test('mergeFailsForRepeatedStore', function() {
    const deviceEntry1 =
        createExceptionEntry({url: 'g.com', id: 0, fromAccountStore: false});
    const deviceEntry2 =
        createExceptionEntry({url: 'g.com', id: 1, fromAccountStore: false});
    assertFalse(
        new MultiStoreExceptionEntry(deviceEntry1).mergeInPlace(deviceEntry2));
  });

  test('mergeFailsForDifferentContents', function() {
    const deviceEntry =
        createExceptionEntry({url: 'a.com', id: 0, fromAccountStore: false});
    const accountEntry =
        createExceptionEntry({url: 'b.com', id: 1, fromAccountStore: true});
    assertFalse(
        new MultiStoreExceptionEntry(deviceEntry).mergeInPlace(accountEntry));
  });
});
