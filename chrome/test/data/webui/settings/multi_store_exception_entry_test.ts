// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MultiStoreExceptionEntry} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {createExceptionEntry} from './passwords_and_autofill_fake_data.js';

suite('MultiStoreExceptionEntry', function() {
  test('verifyIds', function() {
    const entry = createExceptionEntry({url: 'g.com', id: 0});

    const multiStoreDeviceEntry = new MultiStoreExceptionEntry(entry);
    assertTrue(multiStoreDeviceEntry.isPresentOnDevice());
    assertFalse(multiStoreDeviceEntry.isPresentInAccount());
    assertEquals(multiStoreDeviceEntry.getAnyId(), 0);
  });
});
