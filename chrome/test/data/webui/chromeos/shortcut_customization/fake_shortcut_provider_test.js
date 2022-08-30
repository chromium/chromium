// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {fakeAcceleratorConfig, fakeLayoutInfo} from 'chrome://shortcut-customization/fake_data.js';
import {FakeShortcutProvider} from 'chrome://shortcut-customization/fake_shortcut_provider.js';
import {AcceleratorConfigResult, AcceleratorSource, Modifier} from 'chrome://shortcut-customization/shortcut_types.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('fakeShortcutProviderTest', function() {
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
    // AcceleratorSource.ASH is a mutable source.
    return provider.isMutable(AcceleratorSource.ASH).then((result) => {
      assertTrue(result);
      // AcceleratorSource.BROWSER is not a mutable source
      return provider.isMutable(AcceleratorSource.BROWSER).then((result) => {
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
            AcceleratorSource.ASH, /**action=*/ 0, acceleratorKeys)
        .then((result) => {
          assertEquals(AcceleratorConfigResult.SUCCESS, result);
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
            AcceleratorSource.ASH, /**action=*/ 0, oldAcceleratorKeys,
            newAcceleratorKeys)
        .then((result) => {
          assertEquals(AcceleratorConfigResult.SUCCESS, result);
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
        .removeAccelerator(AcceleratorSource.ASH, /**action=*/ 0, accel)
        .then((result) => {
          assertEquals(AcceleratorConfigResult.SUCCESS, result);
        });
  });

  test('RestoreAllDefaultsFake', () => {
    return provider.restoreAllDefaults().then((result) => {
      assertEquals(AcceleratorConfigResult.SUCCESS, result);
    });
  });

  test('RestoreActionDefaultsFake', () => {
    return provider.restoreActionDefaults(AcceleratorSource.ASH, 0)
        .then((result) => {
          assertEquals(AcceleratorConfigResult.SUCCESS, result);
        });
  });
});
