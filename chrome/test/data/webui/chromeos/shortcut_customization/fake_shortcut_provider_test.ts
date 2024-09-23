// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {fakeAcceleratorConfig, fakeLayoutInfo} from 'chrome://shortcut-customization/js/fake_data.js';
import {FakeShortcutProvider} from 'chrome://shortcut-customization/js/fake_shortcut_provider.js';
import {Accelerator, AcceleratorConfigResult, AcceleratorSource, MojoAcceleratorConfig, MojoLayoutInfo} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {AcceleratorResultData, AcceleratorsUpdatedObserverRemote, PolicyUpdatedObserverRemote} from 'chrome://shortcut-customization/mojom-webui/shortcut_customization.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('fakeShortcutProviderTest', function() {
  let provider: FakeShortcutProvider|null = null;

  setup(() => {
    provider = new FakeShortcutProvider();
  });

  teardown(() => {
    provider = null;
  });

  // Fake class that overrides the `onAcceleratorsUpdated` function. This
  // allows us to intercept the request send from the remote and validate
  // the data received.
  class FakeAcceleratorsUpdatedRemote extends
      AcceleratorsUpdatedObserverRemote {
    override onAcceleratorsUpdated(config: MojoAcceleratorConfig) {
      assertDeepEquals(fakeAcceleratorConfig, config);
    }
  }

  // Fake class that overrides the `onCustomizationPolicyUpdated` function. This
  // allows us to intercept the request send from the remote and validate
  // the data received.
  class FakePolicyUpdatedRemote extends PolicyUpdatedObserverRemote {
    override onCustomizationPolicyUpdated() {}
  }

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

  test('ObserveAcceleratorsUpdated', () => {
    // Set the expected value to be returned when `onAcceleratorsUpdated()` is
    // called.
    getProvider().setFakeAcceleratorsUpdated(
        [fakeAcceleratorConfig as MojoAcceleratorConfig]);

    const remote = new FakeAcceleratorsUpdatedRemote();
    getProvider().addObserver(remote);
    // Simulate `onAcceleratorsUpdated()` being called by an observer.
    return getProvider().getAcceleratorsUpdatedPromiseForTesting();
  });

  test('ObservePolicyUpdated', () => {
    // Set the expected value to be returned when
    // `onCustomizationPolicyUpdated()` is called.
    getProvider().setFakePolicyUpdated();

    const remote = new FakePolicyUpdatedRemote();
    getProvider().addPolicyObserver(remote);
    // Simulate `onCustomizationPolicyUpdated()` being called by an observer.
    return getProvider().getPolicyUpdatedPromiseForTesting();
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

  test('AddAcceleratorFake', () => {
    // TODO(jimmyxgong): Remove this test once real data is ready.
    const fakeResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kSuccess,
      shortcutName: null,
    };

    getProvider().setFakeAddAcceleratorResult(fakeResult);

    return getProvider()
        .addAccelerator(
            AcceleratorSource.kAsh,
            /*action_id=*/ 0, {} as Accelerator)
        .then(({result}) => {
          assertEquals(AcceleratorConfigResult.kSuccess, result.result);
        });
  });

  test('ReplaceAcceleratorFake', () => {
    const fakeResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kSuccess,
      shortcutName: null,
    };

    getProvider().setFakeReplaceAcceleratorResult(fakeResult);

    // TODO(jimmyxgong): Remove this test once real data is ready.
    return getProvider()
        .replaceAccelerator(
            AcceleratorSource.kAsh, /*action_id=*/ 0, {} as Accelerator,
            {} as Accelerator)
        .then(({result}) => {
          assertEquals(AcceleratorConfigResult.kSuccess, result.result);
        });
  });

  test('RemoveAcceleratorFake', () => {
    const fakeResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kSuccess,
      shortcutName: null,
    };

    getProvider().setFakeRemoveAcceleratorResult(fakeResult);

    return getProvider().removeAccelerator().then(({result}) => {
      assertEquals(AcceleratorConfigResult.kSuccess, result.result);
    });
  });

  test('RestoreAllDefaultsFake', () => {
    return getProvider().restoreAllDefaults().then(({result}) => {
      assertEquals(AcceleratorConfigResult.kSuccess, result.result);
    });
  });

  test('RestoreDefaultFake', () => {
    const fakeResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kSuccess,
      shortcutName: null,
    };

    getProvider().setFakeRestoreDefaultResult(fakeResult);

    return getProvider()
        .restoreDefault(AcceleratorSource.kAsh, 0)
        .then(({result}) => {
          assertEquals(AcceleratorConfigResult.kSuccess, result.result);
        });
  });
});
