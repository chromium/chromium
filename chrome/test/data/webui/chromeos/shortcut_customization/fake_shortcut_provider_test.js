// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeAcceleratorConfig, fakeLayoutInfo} from 'chrome://shortcut-customization/fake_data.js';
import {FakeShortcutProvider} from 'chrome://shortcut-customization/fake_shortcut_provider.js';
import {AcceleratorConfig, AcceleratorConfigResult, AcceleratorKeys, AcceleratorSource, LayoutInfoList, Modifier} from 'chrome://shortcut-customization/shortcut_types.js';

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

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

  test('GetAllAcceleratorConfigDefaultFake', () => {
    // TODO(zentaro): Remove this test once real data is ready.
    const expected = new Map();
    provider.setFakeAcceleratorConfig(fakeAcceleratorConfig);
    return provider.getAllAcceleratorConfig().then((result) => {
      assertDeepEquals(fakeAcceleratorConfig, result);
    });
  });

  test('GetLayoutInfoEmpty', () => {
    /** @type {!LayoutInfoList} */
    const expected = [];
    provider.setFakeLayoutInfo(expected);
    return provider.getLayoutInfo().then((result) => {
      assertDeepEquals(expected, result);
    });
  });

  test('GetLayoutInfoDefaultFake', () => {
    // TODO(zentaro): Remove this test once real data is ready.
    provider.setFakeLayoutInfo(fakeLayoutInfo);
    return provider.getLayoutInfo().then((result) => {
      assertDeepEquals(fakeLayoutInfo, result);
    });
  });

  test('IsMutableDefaultFake', () => {
    // TODO(jimmyxgong): Remove this test once real data is ready.
    // AcceleratorSource.kAsh is a mutable source.
    return provider.isMutable(AcceleratorSource.kAsh).then((result) => {
      assertTrue(result);
      // AcceleratorSource.kBrowser is not a mutable source
      return provider.isMutable(AcceleratorSource.kBrowser).then((result) => {
        assertFalse(result);
      });
    });
  });

  test('AddUserAcceleratorFake', () => {
    // TODO(jimmyxgong): Remove this test once real data is ready.
    const acceleratorKeys = /** @type {!AcceleratorKeys} */ ({
      modifiers: Modifier.SHIFT,
      key: 79,
      key_display: 'o',
    });
    return provider
        .addUserAccelerator(
            AcceleratorSource.kAsh, /**action=*/ 0, acceleratorKeys)
        .then((result) => {
          assertEquals(AcceleratorConfigResult.kSuccess, result);
        });
  });

  test('ReplaceAcceleratorFake', () => {
    // TODO(jimmyxgong): Remove this test once real data is ready.
    const oldAcceleratorKeys = /** @type {!AcceleratorKeys} */ ({
      modifiers: Modifier.SHIFT,
      key: 79,
      key_display: 'o',
    });

    const newAcceleratorKeys = /** @type {!AcceleratorKeys} */ ({
      modifiers: Modifier.SHIFT,
      key: 80,
      key_display: 'p',
    });

    return provider
        .replaceAccelerator(
            AcceleratorSource.kAsh, /**action=*/ 0, oldAcceleratorKeys,
            newAcceleratorKeys)
        .then((result) => {
          assertEquals(AcceleratorConfigResult.kSuccess, result);
        });
  });

  test('RemoveAcceleratorFake', () => {
    // TODO(jimmyxgong): Remove this test once real data is ready.
    const accel = /** @type {!AcceleratorKeys} */ ({
      modifiers: Modifier.SHIFT,
      key: 79,
      key_display: 'o',
    });

    return provider
        .removeAccelerator(AcceleratorSource.kAsh, /**action=*/ 0, accel)
        .then((result) => {
          assertEquals(AcceleratorConfigResult.kSuccess, result);
        });
  });

  test('RestoreAllDefaultsFake', () => {
    return provider.restoreAllDefaults().then((result) => {
      assertEquals(AcceleratorConfigResult.kSuccess, result);
    });
  });

  test('RestoreActionDefaultsFake', () => {
    return provider.restoreActionDefaults(AcceleratorSource.kAsh, 0)
        .then((result) => {
          assertEquals(AcceleratorConfigResult.kSuccess, result);
        });
  });
}
