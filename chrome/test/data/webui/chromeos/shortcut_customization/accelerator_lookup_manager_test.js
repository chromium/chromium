// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AcceleratorLookupManager} from 'chrome://shortcut-customization/accelerator_lookup_manager.js';
import {fakeAcceleratorConfig, fakeLayoutInfo} from 'chrome://shortcut-customization/fake_data.js';
import {FakeShortcutProvider} from 'chrome://shortcut-customization/fake_shortcut_provider.js';
import {AcceleratorConfig, AcceleratorInfo, AcceleratorKeys, AcceleratorSource, AcceleratorState, LayoutInfoList, Modifier} from 'chrome://shortcut-customization/shortcut_types.js';

import {assertDeepEquals, assertEquals} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

export function acceleratorLookupManagerTest() {
  /** @type {?FakeShortcutProvider} */
  let provider = null;

  /** @type {?AcceleratorLookupManager} */
  let manager = null;

  setup(() => {
    provider = new FakeShortcutProvider();
    manager = AcceleratorLookupManager.getInstance();
  });

  teardown(() => {
    manager.reset();
    provider = null;
  });

  /**
   * @param {AcceleratorSource} source
   * @param {number} action
   * @param {!AcceleratorKeys} oldKeys
   * @param {!AcceleratorKeys} newKeys
   */
  function replaceAndVerify(source, action, oldKeys, newKeys) {
    const uuid = manager.getAcceleratorFromKeys(JSON.stringify(oldKeys));
    manager.replaceAccelerator(source, action, oldKeys, newKeys);

    // Verify that the old accelerator is no longer part of the reverse
    // lookup.
    assertEquals(
        undefined, manager.getAcceleratorFromKeys(JSON.stringify(oldKeys)));
    // Verify the replacement accelerator is in the reverse lookup.
    assertEquals(uuid, manager.getAcceleratorFromKeys(JSON.stringify(newKeys)));
  }

  /**
   * @param {AcceleratorSource} source
   * @param {number} action
   * @param {!AcceleratorKeys} newKeys
   */
  function addAndVerify(source, action, newKeys) {
    manager.addAccelerator(source, action, newKeys);

    // Verify that the new accelerator is in the reverse lookup.
    assertEquals(
        `${source}-${action}`,
        manager.getAcceleratorFromKeys(JSON.stringify(newKeys)));
  }

  test('AcceleratorLookupDefaultFake', () => {
    // TODO(jimmyxgong): Remove this test once real data is ready.
    provider.setFakeAcceleratorConfig(fakeAcceleratorConfig);
    return provider.getAllAcceleratorConfig().then((result) => {
      assertDeepEquals(fakeAcceleratorConfig, result);

      manager.setAcceleratorLookup(result);

      for (const [source, accelMap] of fakeAcceleratorConfig) {
        for (const [action, accelInfos] of accelMap) {
          const actualAccels = manager.getAccelerators(source, action);
          assertDeepEquals(accelInfos, actualAccels);
        }
      }
    });
  });

  test('GetLayoutInfoDefaultFake', () => {
    // TODO(jimmyxgong): Remove this test once real data is ready.
    provider.setFakeLayoutInfo(fakeLayoutInfo);
    return provider.getLayoutInfo().then((result) => {
      assertDeepEquals(fakeLayoutInfo, result);

      manager.setAcceleratorLayoutLookup(result);

      // 2 layout infos for ChromeOS (Window Management, Virtual Desks).
      assertEquals(2, manager.getSubcategories(/*ChromeOS=*/ 0).size);
      // 1 layout infos for Browser (Tabs).
      assertEquals(1, manager.getSubcategories(/*Browser=*/ 1).size);
    });
  });

  test('ReplaceBasicAccelerator', () => {
    provider.setFakeAcceleratorConfig(fakeAcceleratorConfig);
    return provider.getAllAcceleratorConfig().then((result) => {
      assertDeepEquals(fakeAcceleratorConfig, result);

      manager.setAcceleratorLookup(result);

      // Get Snap Window Right accelerator.
      const expectedAction = 1;
      const ashMap = fakeAcceleratorConfig.get(AcceleratorSource.kAsh);
      const snapWindowRightAccels = ashMap.get(expectedAction);
      // Modifier.Alt + key::221 (']')
      const oldAccel = snapWindowRightAccels[0].accelerator;

      const expectedNewAccel = /** @type {!AcceleratorKeys} */ ({
        modifiers: Modifier.CONTROL,
        key: 79,
        key_display: 'o',
      });

      // Sanity check that new accel is not in the reverse lookup.
      assertEquals(
          undefined,
          manager.getAcceleratorFromKeys(JSON.stringify(expectedNewAccel)));

      replaceAndVerify(
          AcceleratorSource.kAsh, expectedAction, oldAccel, expectedNewAccel);

      // Check that the accelerator got updated in the lookup.
      let lookup =
          manager.getAccelerators(AcceleratorSource.kAsh, expectedAction);
      // Replacing a default shortcut should not remove the default. Expect a
      // new accelerator to be added instead.
      assertEquals(2, lookup.length);
      assertEquals(
          JSON.stringify(expectedNewAccel),
          JSON.stringify(lookup[1].accelerator));

      // Replace the new accelerator with the "ALT + ]" default accelerator.
      const expectedNewDefaultAccel = /** @type {!AcceleratorKeys} */ ({
        modifiers: Modifier.ALT,
        key: 221,
        key_display: ']',
      });

      // Sanity check that new accel is not in the reverse lookup.
      assertEquals(
          undefined,
          manager.getAcceleratorFromKeys(
              JSON.stringify(expectedNewDefaultAccel)));
      replaceAndVerify(
          AcceleratorSource.kAsh, expectedAction, expectedNewAccel,
          expectedNewDefaultAccel);

      // Check that the accelerator got updated in the lookup.
      lookup = manager.getAccelerators(AcceleratorSource.kAsh, expectedAction);
      // Expect only one accelerator since the previous accelerator has been
      // removed but the default accelerator has been re-enabled.
      assertEquals(1, lookup.length);
      assertEquals(
          JSON.stringify(expectedNewDefaultAccel),
          JSON.stringify(lookup[0].accelerator));
    });
  });

  test('ReplacePreexistingAccelerator', () => {
    provider.setFakeAcceleratorConfig(fakeAcceleratorConfig);
    return provider.getAllAcceleratorConfig().then((result) => {
      assertDeepEquals(fakeAcceleratorConfig, result);

      manager.setAcceleratorLookup(result);

      // Get Snap Window Right accelerator, the action that will be overridden.
      const snapWindowRightAction = 1;
      const ashMap = fakeAcceleratorConfig.get(AcceleratorSource.kAsh);
      const snapWindowRightAccels = ashMap.get(snapWindowRightAction);
      // Modifier.Alt + key::221 (']')
      const overridenAccel = snapWindowRightAccels[0].accelerator;

      // Replace New Desk shortcut with Alt+']'.
      const newDeskAction = 2;
      const oldNewDeskAccel = ashMap.get(newDeskAction)[0].accelerator;

      replaceAndVerify(
          AcceleratorSource.kAsh, newDeskAction, oldNewDeskAccel,
          overridenAccel);

      // Verify that the New Desk shortcut now has the ALT + ']' accelerator.
      const newDeskLookup =
          manager.getAccelerators(AcceleratorSource.kAsh, newDeskAction);
      assertEquals(2, newDeskLookup.length);
      assertEquals(
          JSON.stringify(overridenAccel),
          JSON.stringify(newDeskLookup[1].accelerator));

      // There should still be 1 accelerator for snapWindowRight, but the
      // default should be disabled.
      const snapWindowRightLookup = manager.getAccelerators(
          AcceleratorSource.kAsh, snapWindowRightAction);
      assertEquals(1, snapWindowRightLookup.length);
      assertEquals(
          AcceleratorState.kDisabledByUser, snapWindowRightLookup[0].state);
    });
  });

  test('AddBasicAccelerator', () => {
    provider.setFakeAcceleratorConfig(fakeAcceleratorConfig);
    return provider.getAllAcceleratorConfig().then((result) => {
      assertDeepEquals(fakeAcceleratorConfig, result);

      manager.setAcceleratorLookup(result);

      // Get Snap Window Right accelerator from kAsh[1].
      const expectedAction = 1;

      const expectedNewAccel = /** @type {!AcceleratorKeys} */ ({
        modifiers: Modifier.CONTROL,
        key: 79,
        key_display: 'o',
      });

      // Sanity check that new accel is not in the reverse lookup.
      assertEquals(
          undefined,
          manager.getAcceleratorFromKeys(JSON.stringify(expectedNewAccel)));

      addAndVerify(AcceleratorSource.kAsh, expectedAction, expectedNewAccel);

      // Check that the accelerator got updated in the lookup.
      const lookup =
          manager.getAccelerators(AcceleratorSource.kAsh, expectedAction);
      assertEquals(2, lookup.length);
      assertEquals(
          JSON.stringify(expectedNewAccel),
          JSON.stringify(lookup[1].accelerator));
    });
  });

  test('AddExistingAccelerator', () => {
    provider.setFakeAcceleratorConfig(fakeAcceleratorConfig);
    return provider.getAllAcceleratorConfig().then((result) => {
      assertDeepEquals(fakeAcceleratorConfig, result);

      manager.setAcceleratorLookup(result);

      // Get Snap Window Right accelerator, the action that will be overridden.
      const snapWindowRightAction = 1;
      const ashMap = fakeAcceleratorConfig.get(AcceleratorSource.kAsh);
      const snapWindowRightAccels = ashMap.get(snapWindowRightAction);
      // Modifier.Alt + key::221 (']')
      const overridenAccel = snapWindowRightAccels[0].accelerator;

      // Replace New Desk shortcut with Alt+']'.
      const newDeskAction = 2;

      addAndVerify(AcceleratorSource.kAsh, newDeskAction, overridenAccel);

      // Verify that the New Desk shortcut now has the ALT + ']' accelerator.
      const newDeskLookup =
          manager.getAccelerators(AcceleratorSource.kAsh, newDeskAction);
      assertEquals(2, newDeskLookup.length);
      assertEquals(
          JSON.stringify(overridenAccel),
          JSON.stringify(newDeskLookup[1].accelerator));

      // Replacing a default accelerator should not remove it but rather disable
      // it.
      const snapWindowRightLookup = manager.getAccelerators(
          AcceleratorSource.kAsh, snapWindowRightAction);
      assertEquals(1, snapWindowRightLookup.length);
      assertEquals(
          AcceleratorState.kDisabledByUser, snapWindowRightLookup[0].state);
    });
  });

  test('RemoveDefaultAccelerator', () => {
    provider.setFakeAcceleratorConfig(fakeAcceleratorConfig);
    return provider.getAllAcceleratorConfig().then((result) => {
      assertDeepEquals(fakeAcceleratorConfig, result);

      manager.setAcceleratorLookup(result);

      // Get Snap Window Right accelerator from kAsh[1].
      const expectedAction = 1;

      // Initially there is only one accelerator for Snap Window Right.
      const lookup =
          manager.getAccelerators(AcceleratorSource.kAsh, expectedAction);
      assertEquals(1, lookup.length);

      // Remove the accelerator.
      const removedAccelerator = lookup[0].accelerator;
      manager.removeAccelerator(
          AcceleratorSource.kAsh, expectedAction, removedAccelerator);

      // Removing a default accelerator only disables it.
      assertEquals(1, lookup.length);
      assertEquals(AcceleratorState.kDisabledByUser, lookup[0].state);

      // Removed accelerator should not appear in the reverse lookup.
      assertEquals(
          undefined,
          manager.getAcceleratorFromKeys(JSON.stringify(removedAccelerator)));
    });
  });

  test('AddAndRemoveAccelerator', () => {
    provider.setFakeAcceleratorConfig(fakeAcceleratorConfig);
    return provider.getAllAcceleratorConfig().then((result) => {
      assertDeepEquals(fakeAcceleratorConfig, result);

      manager.setAcceleratorLookup(result);

      // Get Snap Window Right accelerator from kAsh[1].
      const expectedAction = 1;

      // Initially there is only one accelerator for Snap Window Right.
      const lookup =
          manager.getAccelerators(AcceleratorSource.kAsh, expectedAction);
      assertEquals(1, lookup.length);

      const expectedNewAccel = /** @type {!AcceleratorKeys} */ ({
        modifiers: Modifier.CONTROL,
        key: 79,
        key_display: 'o',
      });

      // Sanity check that new accel is not in the reverse lookup.
      assertEquals(
          undefined,
          manager.getAcceleratorFromKeys(JSON.stringify(expectedNewAccel)));

      addAndVerify(AcceleratorSource.kAsh, expectedAction, expectedNewAccel);

      // Check that the accelerator got updated in the lookup.
      assertEquals(2, lookup.length);
      assertEquals(
          JSON.stringify(expectedNewAccel),
          JSON.stringify(lookup[1].accelerator));

      // Remove the accelerator.
      const removedAccelerator = lookup[1].accelerator;
      manager.removeAccelerator(
          AcceleratorSource.kAsh, expectedAction, removedAccelerator);

      // Expect only 1 accelerator.
      assertEquals(1, lookup.length);

      // Removed accelerator should not appear in the reverse lookup.
      assertEquals(
          undefined,
          manager.getAcceleratorFromKeys(JSON.stringify(removedAccelerator)));
    });
  });
}