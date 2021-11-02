// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for MultiStorePasswordUiEntry.
 */

import {MultiStorePasswordUiEntry} from 'chrome://settings/settings.js';
import {createPasswordEntry} from './passwords_and_autofill_fake_data.js';

suite('MultiStorePasswordUiEntry', function() {
  test('verifyIds', function() {
    const deviceEntry = createPasswordEntry(
        {url: 'g.com', username: 'user', id: 0, fromAccountStore: false});
    const accountEntry = createPasswordEntry(
        {url: 'g.com', username: 'user', id: 1, fromAccountStore: true});

    const multiStoreDeviceEntry = new MultiStorePasswordUiEntry(deviceEntry);
    expectTrue(multiStoreDeviceEntry.isPresentOnDevice());
    expectFalse(multiStoreDeviceEntry.isPresentInAccount());
    expectEquals(multiStoreDeviceEntry.getAnyId(), 0);

    const multiStoreAccountEntry = new MultiStorePasswordUiEntry(accountEntry);
    expectFalse(multiStoreAccountEntry.isPresentOnDevice());
    expectTrue(multiStoreAccountEntry.isPresentInAccount());
    expectEquals(multiStoreAccountEntry.getAnyId(), 1);

    const multiStoreEntryFromBoth = new MultiStorePasswordUiEntry(deviceEntry);
    expectTrue(multiStoreEntryFromBoth.mergeInPlace(accountEntry));
    expectTrue(multiStoreEntryFromBoth.isPresentOnDevice());
    expectTrue(multiStoreEntryFromBoth.isPresentInAccount());
    expectTrue(
        multiStoreEntryFromBoth.getAnyId() === 0 ||
        multiStoreEntryFromBoth.getAnyId() === 1);
  });

  test('mergeFailsForRepeatedStore', function() {
    const deviceEntry1 = createPasswordEntry(
        {url: 'g.com', username: 'user', id: 0, fromAccountStore: false});
    const deviceEntry2 = createPasswordEntry(
        {url: 'g.com', username: 'user', id: 1, fromAccountStore: false});
    expectFalse(
        new MultiStorePasswordUiEntry(deviceEntry1).mergeInPlace(deviceEntry2));
  });

  test('mergeFailsForDifferentContents', function() {
    const deviceEntry = createPasswordEntry(
        {url: 'g.com', username: 'user', id: 0, fromAccountStore: false});
    const accountEntry = createPasswordEntry(
        {url: 'g.com', username: 'user2', id: 1, fromAccountStore: true});
    expectFalse(
        new MultiStorePasswordUiEntry(deviceEntry).mergeInPlace(accountEntry));
  });
});
