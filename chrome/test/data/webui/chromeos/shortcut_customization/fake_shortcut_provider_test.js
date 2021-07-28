// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeShortcutProvider} from 'chrome://shortcut-customization/fake_shortcut_provider.js';
import {AcceleratorConfig} from 'chrome://shortcut-customization/shortcut_types.js';

import {assertDeepEquals} from '../../chai_assert.js';

export function fakeShortcutProviderTest() {
  /** @type {?FakeShortcutProvider} */
  let provider = null;

  setup(() => {
    provider = new FakeShortcutProvider();
  });

  teardown(() => {
    provider = null;
  });

  test('GetAllAcceleratorConfigEmpty', () => {
    /** @type {!AcceleratorConfig} */
    const expected = new Map();
    provider.setFakeAcceleratorConfig(expected);
    return provider.getAllAcceleratorConfig().then((result) => {
      assertDeepEquals(expected, result);
    });
  });
}
