// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {fakeAcceleratorConfig, fakeLayoutInfo} from 'chrome://shortcut-customization/js/fake_data.js';
import {FakeShortcutProvider} from 'chrome://shortcut-customization/js/fake_shortcut_provider.js';
import {AcceleratorConfigResult, AcceleratorSource, LayoutInfoList} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('fakeShortcutProviderTest', function() {
  let provider: FakeShortcutProvider|null = null;

  setup(() => {
    provider = new FakeShortcutProvider();
  });

  teardown(() => {
    provider = null;
  });

  function getProvider(): FakeShortcutProvider {
    assertTrue(!!provider);
    return provider as FakeShortcutProvider;
  }
  test('GetAllAcceleratorConfigEmpty', () => {
    const expected = new Map();
    getProvider().setFakeAcceleratorConfig(expected);
    return getProvider().getAllAcceleratorConfig().then((result) => {
      assertDeepEquals(expected, result);
    });
  });

  test('GetAllAcceleratorConfigDefaultFake', () => {
    // TODO(zentaro): Remove this test once real data is ready.
    getProvider().setFakeAcceleratorConfig(fakeAcceleratorConfig);
    return getProvider().getAllAcceleratorConfig().then((result) => {
      assertDeepEquals(fakeAcceleratorConfig, result);
    });
  });

  test('GetLayoutInfoEmpty', () => {
    const expected: LayoutInfoList = [];
    getProvider().setFakeLayoutInfo(expected);
    return getProvider().getLayoutInfo().then((result) => {
      assertDeepEquals(expected, result);
    });
  });

  test('GetLayoutInfoDefaultFake', () => {
    // TODO(zentaro): Remove this test once real data is ready.
    getProvider().setFakeLayoutInfo(fakeLayoutInfo);
    return getProvider().getLayoutInfo().then((result) => {
      assertDeepEquals(fakeLayoutInfo, result);
    });
  });

  test('IsMutableDefaultFake', () => {
    // TODO(jimmyxgong): Remove this test once real data is ready.
    // AcceleratorSource.ASH is a mutable source.
    return getProvider().isMutable(AcceleratorSource.ASH).then((result) => {
      assertTrue(result);
      // AcceleratorSource.BROWSER is not a mutable source
      return getProvider()
          .isMutable(AcceleratorSource.BROWSER)
          .then((result) => {
            assertFalse(result);
          });
    });
  });

  test('AddUserAcceleratorFake', () => {
    // TODO(jimmyxgong): Remove this test once real data is ready.
    return getProvider().addUserAccelerator().then((result) => {
      assertEquals(AcceleratorConfigResult.SUCCESS, result);
    });
  });

  test('ReplaceAcceleratorFake', () => {
    // TODO(jimmyxgong): Remove this test once real data is ready.
    return getProvider().replaceAccelerator().then((result) => {
      assertEquals(AcceleratorConfigResult.SUCCESS, result);
    });
  });

  test('RemoveAcceleratorFake', () => {
    // TODO(jimmyxgong): Remove this test once real data is ready.
    return getProvider().removeAccelerator().then((result) => {
      assertEquals(AcceleratorConfigResult.SUCCESS, result);
    });
  });

  test('RestoreAllDefaultsFake', () => {
    return getProvider().restoreAllDefaults().then((result) => {
      assertEquals(AcceleratorConfigResult.SUCCESS, result);
    });
  });

  test('RestoreActionDefaultsFake', () => {
    return getProvider().restoreActionDefaults().then((result) => {
      assertEquals(AcceleratorConfigResult.SUCCESS, result);
    });
  });
});
