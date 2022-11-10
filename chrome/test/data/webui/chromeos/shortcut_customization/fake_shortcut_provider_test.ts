// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {fakeAcceleratorConfig, fakeLayoutInfo} from 'chrome://shortcut-customization/js/fake_data.js';
import {FakeShortcutProvider} from 'chrome://shortcut-customization/js/fake_shortcut_provider.js';
import {AcceleratorConfigResult, AcceleratorSource, MojoLayoutInfo} from 'chrome://shortcut-customization/js/shortcut_types.js';
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
  test('GetAcceleratorsEmpty', () => {
    const expected = {};
    getProvider().setFakeAcceleratorConfig(expected);
    return getProvider().getAccelerators().then((result) => {
      assertDeepEquals(expected, result.config);
    });
  });

  test('GetAcceleratorsDefaultFake', () => {
    // TODO(zentaro): Remove this test once real data is ready.
    getProvider().setFakeAcceleratorConfig(fakeAcceleratorConfig);
    return getProvider().getAccelerators().then((result) => {
      assertDeepEquals(fakeAcceleratorConfig, result.config);
    });
  });

  test('GetLayoutInfoEmpty', () => {
    const expected: MojoLayoutInfo[] = [];
    getProvider().setFakeAcceleratorLayoutInfos(expected);
    return getProvider().getAcceleratorLayoutInfos().then((result) => {
      assertDeepEquals(expected, result.layoutInfos);
    });
  });

  test('GetLayoutInfoDefaultFake', () => {
    // TODO(zentaro): Remove this test once real data is ready.
    getProvider().setFakeAcceleratorLayoutInfos(fakeLayoutInfo);
    return getProvider().getAcceleratorLayoutInfos().then((result) => {
      assertDeepEquals(fakeLayoutInfo, result.layoutInfos);
    });
  });

  test('IsMutableDefaultFake', () => {
    // TODO(jimmyxgong): Remove this test once real data is ready.
    // AcceleratorSource.kAsh is a mutable source.
    return getProvider().isMutable(AcceleratorSource.kAsh).then((result) => {
      assertTrue(result.isMutable);
      // AcceleratorSource.kBrowser is not a mutable source
      return getProvider()
          .isMutable(AcceleratorSource.kBrowser)
          .then((result) => {
            assertFalse(result.isMutable);
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
