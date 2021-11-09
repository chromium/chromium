// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeUpdateController} from 'chrome://accessory-update/fake_update_controller.js';

import {assertTrue} from '../../chai_assert.js';

export function fakeUpdateControllerTest() {
  /** @type {?FakeUpdateController} */
  let controller = null;

  setup(() => controller = new FakeUpdateController());

  teardown(() => controller = null);

  test('InstantiateFakeUpdateController', () => {
    // TODO(michaelcheco): Remove when a method on this interface is
    // implemented.
    assertTrue(!!controller);
  });
}
