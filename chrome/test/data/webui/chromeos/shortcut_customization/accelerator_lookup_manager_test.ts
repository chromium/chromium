// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {AcceleratorLookupManager} from 'chrome://shortcut-customization/js/accelerator_lookup_manager.js';
import {fakeAcceleratorConfig, fakeLayoutInfo} from 'chrome://shortcut-customization/js/fake_data.js';
import {FakeShortcutProvider} from 'chrome://shortcut-customization/js/fake_shortcut_provider.js';
import {AcceleratorInfo, AcceleratorKeys, AcceleratorSource, AcceleratorState, Modifier} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('acceleratorLookupManagerTest', function() {
  let provider: FakeShortcutProvider|null = null;

  let manager: AcceleratorLookupManager|null = null;

  setup(() => {
    provider = new FakeShortcutProvider();
    manager = AcceleratorLookupManager.getInstance();
  });

  teardown(() => {
    if (manager) {
      manager.reset();
    }
    provider = null;
  });

  function getManager(): AcceleratorLookupManager {
    assertTrue(!!manager);
    return manager;
  }

  function getProvider(): FakeShortcutProvider {
    assertTrue(!!provider);
    return provider;
  }

  function replaceAndVerify(
      source: AcceleratorSource, action: number, oldKeys: AcceleratorKeys,
      newKeys: AcceleratorKeys) {
    const uuid = getManager().getAcceleratorFromKeys(JSON.stringify(oldKeys));
    getManager().replaceAccelerator(source, action, oldKeys, newKeys);

    // Verify that the old accelerator is no longer part of the reverse
    // lookup.
    assertEquals(
        undefined,
        getManager().getAcceleratorFromKeys(JSON.stringify(oldKeys)));
    // Verify the replacement accelerator is in the reverse lookup.
    assertEquals(
        uuid, getManager().getAcceleratorFromKeys(JSON.stringify(newKeys)));
  }

  function addAndVerify(
      source: AcceleratorSource, action: number, newKeys: AcceleratorKeys) {
    getManager().addAccelerator(source, action, newKeys);

    // Verify that the new accelerator is in the reverse lookup.
    assertEquals(
        `${source}-${action}`,
        getManager().getAcceleratorFromKeys(JSON.stringify(newKeys)));
  }

  test('AcceleratorLookupDefaultFake', () => {
    // TODO(jimmyxgong): Remove this test once real data is ready.
    getProvider().setFakeAcceleratorConfig(fakeAcceleratorConfig);
    return getProvider().getAllAcceleratorConfig().then((result) => {
      assertDeepEquals(fakeAcceleratorConfig, result);

      getManager().setAcceleratorLookup(result);

      for (const [source, accelMap] of fakeAcceleratorConfig) {
        for (const [action, accelInfos] of accelMap) {
          const actualAccels = getManager().getAccelerators(source, action);
          assertDeepEquals(accelInfos, actualAccels);
        }
      }
    });
  });

  test('GetLayoutInfoDefaultFake', () => {
    // TODO(jimmyxgong): Remove this test once real data is ready.
    getProvider().setFakeLayoutInfo(fakeLayoutInfo);
    return getProvider().getLayoutInfo().then((result) => {
      assertDeepEquals(fakeLayoutInfo, result);

      getManager().setAcceleratorLayoutLookup(result);

      // 2 layout infos for ChromeOS (Window Management, Virtual Desks).
      assertEquals(2, getManager().getSubcategories(/*ChromeOS=*/ 0)!.size);
      // 1 layout infos for Browser (Tabs).
      assertEquals(1, getManager().getSubcategories(/*Browser=*/ 1)!.size);
    });
  });

  test('ReplaceBasicAccelerator', () => {
    getProvider().setFakeAcceleratorConfig(fakeAcceleratorConfig);
    return getProvider().getAllAcceleratorConfig().then((result) => {
      assertDeepEquals(fakeAcceleratorConfig, result);

      getManager().setAcceleratorLookup(result);

      // Get Snap Window Right accelerator.
      const expectedAction = 1;
      const ashMap = fakeAcceleratorConfig.get(AcceleratorSource.ASH);
      const snapWindowRightAccels = ashMap!.get(expectedAction);
      assertTrue(!!snapWindowRightAccels);
      // Modifier.Alt + key::221 (']')
      const oldAccel = snapWindowRightAccels[0]!.accelerator;

      const expectedNewAccel = /** @type {!AcceleratorKeys} */ ({
        modifiers: Modifier.CONTROL,
        key: 79,
        keyDisplay: 'o',
      });

      // Sanity check that new accel is not in the reverse lookup.
      assertEquals(
          undefined,
          getManager().getAcceleratorFromKeys(
              JSON.stringify(expectedNewAccel)));

      replaceAndVerify(
          AcceleratorSource.ASH, expectedAction, oldAccel, expectedNewAccel);

      // Check that the accelerator got updated in the lookup.
      let lookup =
          getManager().getAccelerators(AcceleratorSource.ASH, expectedAction);
      // Replacing a default shortcut should not remove the default. Expect a
      // new accelerator to be added instead.
      assertEquals(2, lookup.length);
      assertEquals(
          JSON.stringify(expectedNewAccel),
          JSON.stringify(lookup[1]!.accelerator));

      // Replace the new accelerator with the "ALT + ]" default accelerator.
      const expectedNewDefaultAccel = /** @type {!AcceleratorKeys} */ ({
        modifiers: Modifier.ALT,
        key: 221,
        keyDisplay: ']',
      });

      // Sanity check that new accel is not in the reverse lookup.
      assertEquals(
          undefined,
          getManager().getAcceleratorFromKeys(
              JSON.stringify(expectedNewDefaultAccel)));
      replaceAndVerify(
          AcceleratorSource.ASH, expectedAction, expectedNewAccel,
          expectedNewDefaultAccel);

      // Check that the accelerator got updated in the lookup.
      lookup =
          getManager().getAccelerators(AcceleratorSource.ASH, expectedAction);
      // Expect only one accelerator since the previous accelerator has been
      // removed but the default accelerator has been re-enabled.
      assertEquals(1, lookup.length);
      assertEquals(
          JSON.stringify(expectedNewDefaultAccel),
          JSON.stringify(lookup[0]!.accelerator));
    });
  });

  test('ReplacePreexistingAccelerator', () => {
    getProvider().setFakeAcceleratorConfig(fakeAcceleratorConfig);
    return getProvider().getAllAcceleratorConfig().then((result) => {
      assertDeepEquals(fakeAcceleratorConfig, result);

      getManager().setAcceleratorLookup(result);

      // Get Snap Window Right accelerator, the action that will be overridden.
      const snapWindowRightAction = 1;
      const ashMap = fakeAcceleratorConfig!.get(AcceleratorSource.ASH) as
          Map<number, AcceleratorInfo[]>;
      const snapWindowRightAccels =
          ashMap!.get(snapWindowRightAction) as AcceleratorInfo[];
      // Modifier.Alt + key::221 (']')
      const overridenAccel = snapWindowRightAccels[0]!.accelerator;

      // Replace New Desk shortcut with Alt+']'.
      const newDeskAction = 2;
      const oldNewDeskAccels = ashMap!.get(newDeskAction) as AcceleratorInfo[];
      const oldNewDeskAccel = oldNewDeskAccels[0]!.accelerator;

      replaceAndVerify(
          AcceleratorSource.ASH, newDeskAction, oldNewDeskAccel,
          overridenAccel);

      // Verify that the New Desk shortcut now has the ALT + ']' accelerator.
      const newDeskLookup =
          getManager().getAccelerators(AcceleratorSource.ASH, newDeskAction);
      assertEquals(2, newDeskLookup.length);
      assertEquals(
          JSON.stringify(overridenAccel),
          JSON.stringify(newDeskLookup[1]!.accelerator));

      // There should still be 1 accelerator for snapWindowRight, but the
      // default should be disabled.
      const snapWindowRightLookup = getManager().getAccelerators(
          AcceleratorSource.ASH, snapWindowRightAction);
      assertEquals(1, snapWindowRightLookup.length);
      assertEquals(
          AcceleratorState.DISABLED_BY_USER, snapWindowRightLookup[0]!.state);
    });
  });

  test('AddBasicAccelerator', () => {
    getProvider().setFakeAcceleratorConfig(fakeAcceleratorConfig);
    return getProvider().getAllAcceleratorConfig().then((result) => {
      assertDeepEquals(fakeAcceleratorConfig, result);

      getManager().setAcceleratorLookup(result);

      // Get Snap Window Right accelerator from kAsh[1]!.
      const expectedAction = 1;

      const expectedNewAccel = /** @type {!AcceleratorKeys} */ ({
        modifiers: Modifier.CONTROL,
        key: 79,
        keyDisplay: 'o',
      });

      // Sanity check that new accel is not in the reverse lookup.
      assertEquals(
          undefined,
          getManager().getAcceleratorFromKeys(
              JSON.stringify(expectedNewAccel)));

      addAndVerify(AcceleratorSource.ASH, expectedAction, expectedNewAccel);

      // Check that the accelerator got updated in the lookup.
      const lookup =
          getManager().getAccelerators(AcceleratorSource.ASH, expectedAction);
      assertEquals(2, lookup.length);
      assertEquals(
          JSON.stringify(expectedNewAccel),
          JSON.stringify(lookup[1]!.accelerator));
    });
  });

  test('AddExistingAccelerator', () => {
    getProvider().setFakeAcceleratorConfig(fakeAcceleratorConfig);
    return getProvider().getAllAcceleratorConfig().then((result) => {
      assertDeepEquals(fakeAcceleratorConfig, result);

      getManager().setAcceleratorLookup(result);

      // Get Snap Window Right accelerator, the action that will be overridden.
      const snapWindowRightAction = 1;
      const ashMap = fakeAcceleratorConfig!.get(AcceleratorSource.ASH) as
          Map<number, AcceleratorInfo[]>;
      const snapWindowRightAccels =
          ashMap!.get(snapWindowRightAction) as AcceleratorInfo[];
      // Modifier.Alt + key::221 (']')
      const overridenAccel = snapWindowRightAccels[0]!.accelerator;

      // Replace New Desk shortcut with Alt+']'.
      const newDeskAction = 2;

      addAndVerify(AcceleratorSource.ASH, newDeskAction, overridenAccel);

      // Verify that the New Desk shortcut now has the ALT + ']' accelerator.
      const newDeskLookup =
          getManager().getAccelerators(AcceleratorSource.ASH, newDeskAction);
      assertEquals(2, newDeskLookup.length);
      assertEquals(
          JSON.stringify(overridenAccel),
          JSON.stringify(newDeskLookup[1]!.accelerator));

      // Replacing a default accelerator should not remove it but rather disable
      // it.
      const snapWindowRightLookup = getManager().getAccelerators(
          AcceleratorSource.ASH, snapWindowRightAction);
      assertEquals(1, snapWindowRightLookup.length);
      assertEquals(
          AcceleratorState.DISABLED_BY_USER, snapWindowRightLookup[0]!.state);
    });
  });

  test('RemoveDefaultAccelerator', () => {
    getProvider().setFakeAcceleratorConfig(fakeAcceleratorConfig);
    return getProvider().getAllAcceleratorConfig().then((result) => {
      assertDeepEquals(fakeAcceleratorConfig, result);

      getManager().setAcceleratorLookup(result);

      // Get Snap Window Right accelerator from kAsh[1].
      const expectedAction = 1;

      // Initially there is only one accelerator for Snap Window Right.
      const lookup =
          getManager().getAccelerators(AcceleratorSource.ASH, expectedAction);
      assertEquals(1, lookup.length);

      // Remove the accelerator.
      const removedAccelerator = lookup[0]!.accelerator;
      getManager().removeAccelerator(
          AcceleratorSource.ASH, expectedAction, removedAccelerator);

      // Removing a default accelerator only disables it.
      assertEquals(1, lookup.length);
      assertEquals(AcceleratorState.DISABLED_BY_USER, lookup[0]!.state);

      // Removed accelerator should not appear in the reverse lookup.
      assertEquals(
          undefined,
          getManager().getAcceleratorFromKeys(
              JSON.stringify(removedAccelerator)));
    });
  });

  test('AddAndRemoveAccelerator', () => {
    getProvider().setFakeAcceleratorConfig(fakeAcceleratorConfig);
    return getProvider().getAllAcceleratorConfig().then((result) => {
      assertDeepEquals(fakeAcceleratorConfig, result);

      getManager().setAcceleratorLookup(result);

      // Get Snap Window Right accelerator from kAsh[1]!.
      const expectedAction = 1;

      // Initially there is only one accelerator for Snap Window Right.
      const lookup =
          getManager().getAccelerators(AcceleratorSource.ASH, expectedAction);
      assertEquals(1, lookup.length);

      const expectedNewAccel = /** @type {!AcceleratorKeys} */ ({
        modifiers: Modifier.CONTROL,
        key: 79,
        keyDisplay: 'o',
      });

      // Sanity check that new accel is not in the reverse lookup.
      assertEquals(
          undefined,
          getManager().getAcceleratorFromKeys(
              JSON.stringify(expectedNewAccel)));

      addAndVerify(AcceleratorSource.ASH, expectedAction, expectedNewAccel);

      // Check that the accelerator got updated in the lookup.
      assertEquals(2, lookup.length);
      assertEquals(
          JSON.stringify(expectedNewAccel),
          JSON.stringify(lookup[1]!.accelerator));

      // Remove the accelerator.
      const removedAccelerator = lookup[1]!.accelerator;
      getManager().removeAccelerator(
          AcceleratorSource.ASH, expectedAction, removedAccelerator);

      // Expect only 1 accelerator.
      assertEquals(1, lookup.length);

      // Removed accelerator should not appear in the reverse lookup.
      assertEquals(
          undefined,
          getManager().getAcceleratorFromKeys(
              JSON.stringify(removedAccelerator)));
    });
  });
});