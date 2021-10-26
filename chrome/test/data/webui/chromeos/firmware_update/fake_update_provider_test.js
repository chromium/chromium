// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeUpdateProvider} from 'chrome://accessory-update/fake_update_provider.js';

import {assertTrue} from '../../chai_assert.js';

export function fakeUpdateProviderTest() {
  /** @type {?FakeUpdateProvider} */
  let provider = null;

  setup(() => provider = new FakeUpdateProvider());

  teardown(() => provider = null);

  test('InstantiateFakeUpdateProvider', () => {
    // TODO(michaelcheco): Remove when a method on this interface is
    // implemented.
    assertTrue(!!provider);
  });
}
