// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MultiStorePasswordUiEntry} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {createPasswordEntry} from './passwords_and_autofill_fake_data.js';

suite('MultiStorePasswordUiEntry', function() {
  test('verifyIds', function() {
    const deviceEntry = createPasswordEntry(
        {url: 'g.com', username: 'user', id: 0, fromAccountStore: false});
    const accountEntry = createPasswordEntry(
        {url: 'g.com', username: 'user', id: 1, fromAccountStore: true});

    const multiStoreDeviceEntry = new MultiStorePasswordUiEntry(deviceEntry);
    assertTrue(multiStoreDeviceEntry.isPresentOnDevice());
    assertFalse(multiStoreDeviceEntry.isPresentInAccount());
    assertEquals(multiStoreDeviceEntry.getAnyId(), 0);

    const multiStoreAccountEntry = new MultiStorePasswordUiEntry(accountEntry);
    assertFalse(multiStoreAccountEntry.isPresentOnDevice());
    assertTrue(multiStoreAccountEntry.isPresentInAccount());
    assertEquals(multiStoreAccountEntry.getAnyId(), 1);

    const multiStoreEntryFromBoth = new MultiStorePasswordUiEntry(deviceEntry);
    assertTrue(multiStoreEntryFromBoth.mergeInPlace(accountEntry));
    assertTrue(multiStoreEntryFromBoth.isPresentOnDevice());
    assertTrue(multiStoreEntryFromBoth.isPresentInAccount());
    assertTrue(
        multiStoreEntryFromBoth.getAnyId() === 0 ||
        multiStoreEntryFromBoth.getAnyId() === 1);
  });

  test('mergeFailsForRepeatedStore', function() {
    const deviceEntry1 = createPasswordEntry(
        {url: 'g.com', username: 'user', id: 0, fromAccountStore: false});
    const deviceEntry2 = createPasswordEntry(
        {url: 'g.com', username: 'user', id: 1, fromAccountStore: false});
    assertFalse(
        new MultiStorePasswordUiEntry(deviceEntry1).mergeInPlace(deviceEntry2));
  });

  test('mergeFailsForDifferentContents', function() {
    const deviceEntry = createPasswordEntry(
        {url: 'g.com', username: 'user', id: 0, fromAccountStore: false});
    const accountEntry = createPasswordEntry(
        {url: 'g.com', username: 'user2', id: 1, fromAccountStore: true});
    assertFalse(
        new MultiStorePasswordUiEntry(deviceEntry).mergeInPlace(accountEntry));
  });
});
