// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {AcceleratorLookupManager} from 'chrome://shortcut-customization/js/accelerator_lookup_manager.js';
import {fakeAcceleratorConfig, fakeLayoutInfo} from 'chrome://shortcut-customization/js/fake_data.js';
import {FakeShortcutProvider} from 'chrome://shortcut-customization/js/fake_shortcut_provider.js';
import {Accelerator, AcceleratorCategory, AcceleratorSource, AcceleratorState, DefaultAcceleratorInfo, Modifier, MojoAccelerator, MojoAcceleratorInfo} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {areAcceleratorsEqual, createEmptyAccelInfoFromAccel} from 'chrome://shortcut-customization/js/shortcut_utils.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

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
      source: AcceleratorSource, action: number, oldAccel: Accelerator,
      newAccelInfo: DefaultAcceleratorInfo) {
    const uuid = getManager().getAcceleratorIdFromReverseLookup(oldAccel);
    getManager().replaceAccelerator(source, action, oldAccel, newAccelInfo);

    // Verify that the old accelerator is no longer part of the reverse
    // lookup.
    assertEquals(
        undefined, getManager().getAcceleratorIdFromReverseLookup(oldAccel));
    // Verify the replacement accelerator is in the reverse lookup.
    assertEquals(
        uuid,
        getManager().getAcceleratorIdFromReverseLookup(
            newAccelInfo.layoutProperties.defaultAccelerator.accelerator));
  }

  function addAndVerify(
      source: AcceleratorSource, action: number,
      newAccelInfo: DefaultAcceleratorInfo) {
    getManager().addAccelerator(source, action, newAccelInfo);

    // Verify that the new accelerator is in the reverse lookup.
    assertEquals(
        `${source}-${action}`,
        getManager().getAcceleratorIdFromReverseLookup(
            newAccelInfo.layoutProperties.defaultAccelerator.accelerator));
  }

  test('AcceleratorLookupDefaultFake', () => {
    // TODO(jimmyxgong): Remove this test once real data is ready.
    getProvider().setFakeAcceleratorConfig(fakeAcceleratorConfig);
    return getProvider().getAccelerators().then((result) => {
      assertDeepEquals(fakeAcceleratorConfig, result.config);

      getManager().setAcceleratorLookup(result.config);

      for (const [source, accelMap] of Object.entries(fakeAcceleratorConfig)) {
        // When calling Object.entries on an object with optional enum keys,
        // TypeScript considers the values to be possibly undefined.
        // This guard lets us use this value later as if it were not undefined.
        if (!accelMap) {
          continue;
        }
        for (const [action, configAccelInfoArr] of Object.entries(accelMap)) {
          const managerAccelInfoArr =
              getManager().getAcceleratorInfos(source, action);
          // The AcceleratorLookupManager processes the MojoAcceleratorConfig
          // into an AcceleratorConfig. Since the Mojo types (MojoAccelerator,
          // MojoAcceleratorInfo) have different properties from the non-Mojo
          // types, we only expect the properties in common to be equal.
          for (let i = 0; i < configAccelInfoArr.length; i++) {
            const managerAccel =
                managerAccelInfoArr[i] as DefaultAcceleratorInfo;
            const configAccel: MojoAcceleratorInfo =
                configAccelInfoArr[i] as MojoAcceleratorInfo;
            assertEquals(managerAccel.type, configAccel.type);
            assertEquals(managerAccel.locked, configAccel.locked);
            assertEquals(managerAccel.state, configAccel.state);
            assertNotEquals(
                managerAccel.layoutProperties.defaultAccelerator.keyDisplay,
                configAccel.layoutProperties?.defaultAccelerator?.keyDisplay);
            assertEquals(
                managerAccel.layoutProperties.defaultAccelerator.accelerator
                    .keyCode,
                configAccel?.layoutProperties?.defaultAccelerator?.accelerator
                    .keyCode);
            assertEquals(
                managerAccel.layoutProperties.defaultAccelerator.accelerator
                    .modifiers,
                configAccel?.layoutProperties?.defaultAccelerator?.accelerator
                    .modifiers);
            assertFalse(Object.hasOwn(
                (managerAccel.layoutProperties.defaultAccelerator.accelerator as
                 Accelerator),
                'keyState'));
            assertTrue(Object.hasOwn(
                (configAccel.layoutProperties.defaultAccelerator!.accelerator as
                 Accelerator),
                'keyState'));
            assertFalse(Object.hasOwn(
                (managerAccel.layoutProperties.defaultAccelerator.accelerator as
                 Accelerator),
                'timeStamp'));
            assertTrue(Object.hasOwn(
                (configAccel.layoutProperties.defaultAccelerator!.accelerator as
                 Accelerator),
                'timeStamp'));
          }
        }
      }
    });
  });

  test('GetLayoutInfoDefaultFakeWithAccelerators', async () => {
    // TODO(jimmyxgong): Remove this test once real data is ready.

    // First, initialize the accelerators into the AcceleratorLookupManager.
    getProvider().setFakeAcceleratorConfig(fakeAcceleratorConfig);
    const {config: accelConfig} = await getProvider().getAccelerators();
    getManager().setAcceleratorLookup(accelConfig);

    // Then, initialize the layout infos into the AcceleratorLookupManager.
    getProvider().setFakeAcceleratorLayoutInfos(fakeLayoutInfo);
    return getProvider().getAcceleratorLayoutInfos().then((result) => {
      assertDeepEquals(fakeLayoutInfo, result.layoutInfos);

      getManager().setAcceleratorLayoutLookup(result.layoutInfos);

      // We expect 2 subcategories for kTabsAndWindows: kGeneral and
      // kSystemApps.
      assertEquals(
          2,
          getManager().getSubcategories(
                          AcceleratorCategory.kTabsAndWindows)!.size);
      // We expect 1 subcategory for kWindowsAndDesk: kSystemControl.
      assertEquals(
          1,
          getManager().getSubcategories(
                          AcceleratorCategory.kPageAndWebBrowser)!.size);
    });
  });

  test('GetLayoutInfoDefaultFakeNoAccelerators', () => {
    // TODO(jimmyxgong): Remove this test once real data is ready.
    getProvider().setFakeAcceleratorLayoutInfos(fakeLayoutInfo);
    return getProvider().getAcceleratorLayoutInfos().then((result) => {
      assertDeepEquals(fakeLayoutInfo, result.layoutInfos);

      getManager().setAcceleratorLayoutLookup(result.layoutInfos);

      // If accelerators have not been initialized into the
      // AcceleratorLookupManager, we expect the subcategories to be undefined.
      assertEquals(
          undefined,
          getManager().getSubcategories(AcceleratorCategory.kTabsAndWindows));
      assertEquals(
          undefined,
          getManager().getSubcategories(
              AcceleratorCategory.kPageAndWebBrowser));
    });
  });

  test('ReplaceBasicAccelerator', () => {
    getProvider().setFakeAcceleratorConfig(fakeAcceleratorConfig);
    return getProvider().getAccelerators().then((result) => {
      assertDeepEquals(fakeAcceleratorConfig, result.config);

      getManager().setAcceleratorLookup(result.config);

      // Get Snap Window Right accelerator.
      const expectedAction = 1;
      const ashMap = fakeAcceleratorConfig[AcceleratorSource.kAsh];
      const snapWindowRightAccels = ashMap![expectedAction];
      assertTrue(!!snapWindowRightAccels);
      // Modifier.Alt + key::221 (']')
      const oldAccel = snapWindowRightAccels[0]!.layoutProperties!
                           .defaultAccelerator!.accelerator;

      const expectedNewAccel: Accelerator = {
        modifiers: Modifier.CONTROL,
        keyCode: 79,
      };

      // Sanity check that new accel is not in the reverse lookup.
      assertEquals(
          undefined,
          getManager().getAcceleratorIdFromReverseLookup(expectedNewAccel));

      replaceAndVerify(
          AcceleratorSource.kAsh, expectedAction, oldAccel,
          createEmptyAccelInfoFromAccel(expectedNewAccel));

      // Check that the accelerator got updated in the lookup.
      let lookup = getManager().getAcceleratorInfos(
          AcceleratorSource.kAsh, expectedAction);
      // Replacing a default shortcut should not remove the default. Expect
      // a new accelerator to be added instead.
      assertEquals(2, lookup.length);
      assertTrue(areAcceleratorsEqual(
          expectedNewAccel,
          lookup[1]!.layoutProperties.defaultAccelerator.accelerator));

      // Replace the new accelerator with the "ALT + ]" default accelerator.
      const expectedNewDefaultAccel: Accelerator = {
        modifiers: Modifier.ALT,
        keyCode: 221,
      };

      // Sanity check that new accel is not in the reverse lookup.
      assertEquals(
          undefined,
          getManager().getAcceleratorIdFromReverseLookup(
              expectedNewDefaultAccel));
      replaceAndVerify(
          AcceleratorSource.kAsh, expectedAction, expectedNewAccel,
          createEmptyAccelInfoFromAccel(expectedNewDefaultAccel));

      // Check that the accelerator got updated in the lookup.
      lookup = getManager().getAcceleratorInfos(
          AcceleratorSource.kAsh, expectedAction);
      // Expect only one accelerator since the previous accelerator has been
      // removed but the default accelerator has been re-enabled.
      assertEquals(1, lookup.length);
      assertTrue(areAcceleratorsEqual(
          expectedNewDefaultAccel,
          lookup[0]!.layoutProperties.defaultAccelerator.accelerator));
    });
  });

  test('ReplacePreexistingAccelerator', () => {
    getProvider().setFakeAcceleratorConfig(fakeAcceleratorConfig);
    return getProvider().getAccelerators().then((result) => {
      assertDeepEquals(fakeAcceleratorConfig, result.config);

      getManager().setAcceleratorLookup(result.config);

      // Get Snap Window Right accelerator, the action that will be overridden.
      const snapWindowRightAction = 1;
      const ashMap = fakeAcceleratorConfig![AcceleratorSource.kAsh];
      const snapWindowRightAccels = ashMap![snapWindowRightAction];
      // Modifier.Alt + key::221 (']')
      const overridenAccel = snapWindowRightAccels![0]!.layoutProperties!
                                 .defaultAccelerator!.accelerator;

      // Replace New Desk shortcut with Alt+']'.
      const newDeskAction = 2;
      const oldNewDeskAccels = ashMap![newDeskAction];
      const oldNewDeskAccel = oldNewDeskAccels![0]!.layoutProperties!
                                  .defaultAccelerator!.accelerator;

      replaceAndVerify(
          AcceleratorSource.kAsh, newDeskAction, oldNewDeskAccel,
          createEmptyAccelInfoFromAccel(overridenAccel));

      // Verify that the New Desk shortcut now has the ALT + ']' accelerator.
      const newDeskLookup = getManager().getAcceleratorInfos(
          AcceleratorSource.kAsh, newDeskAction);
      assertEquals(2, newDeskLookup.length);
      assertTrue(areAcceleratorsEqual(
          overridenAccel,
          newDeskLookup[1]!.layoutProperties.defaultAccelerator.accelerator));

      // There should still be 1 accelerator for snapWindowRight, but the
      // default should be disabled.
      const snapWindowRightLookup = getManager().getAcceleratorInfos(
          AcceleratorSource.kAsh, snapWindowRightAction);
      assertEquals(1, snapWindowRightLookup.length);
      assertEquals(
          AcceleratorState.kDisabledByUser, snapWindowRightLookup[0]!.state);
    });
  });

  test('AddBasicAccelerator', () => {
    getProvider().setFakeAcceleratorConfig(fakeAcceleratorConfig);
    return getProvider().getAccelerators().then((result) => {
      assertDeepEquals(fakeAcceleratorConfig, result.config);

      getManager().setAcceleratorLookup(result.config);

      // Get Snap Window Right accelerator from kAsh[1]!.
      const expectedAction = 1;

      const expectedNewAccel: Accelerator = {
        modifiers: Modifier.CONTROL,
        keyCode: 79,
      };

      // Sanity check that new accel is not in the reverse lookup.
      assertEquals(
          undefined,
          getManager().getAcceleratorIdFromReverseLookup(expectedNewAccel));

      addAndVerify(
          AcceleratorSource.kAsh, expectedAction,
          createEmptyAccelInfoFromAccel(expectedNewAccel));

      // Check that the accelerator got updated in the lookup.
      const lookup = getManager().getAcceleratorInfos(
          AcceleratorSource.kAsh, expectedAction);
      assertEquals(2, lookup.length);
      assertTrue(areAcceleratorsEqual(
          expectedNewAccel,
          lookup[1]!.layoutProperties.defaultAccelerator.accelerator));
    });
  });

  test('AddExistingAccelerator', () => {
    getProvider().setFakeAcceleratorConfig(fakeAcceleratorConfig);
    return getProvider().getAccelerators().then((result) => {
      assertDeepEquals(fakeAcceleratorConfig, result.config);

      getManager().setAcceleratorLookup(result.config);

      // Get Snap Window Right accelerator, the action that will be overridden.
      const snapWindowRightAction = 1;
      const ashMap = fakeAcceleratorConfig[AcceleratorSource.kAsh];
      const snapWindowRightAccels =
          ashMap![snapWindowRightAction] as MojoAcceleratorInfo[];
      // Modifier.Alt + key::221 (']')
      const overridenAccel =
          snapWindowRightAccels[0]!.layoutProperties!.defaultAccelerator!
              .accelerator as MojoAccelerator;

      // Replace New Desk shortcut with Alt+']'.
      const newDeskAction = 2;

      addAndVerify(
          AcceleratorSource.kAsh, newDeskAction,
          createEmptyAccelInfoFromAccel(overridenAccel));

      // Verify that the New Desk shortcut now has the ALT + ']' accelerator.
      const newDeskLookup = getManager().getAcceleratorInfos(
          AcceleratorSource.kAsh, newDeskAction);
      assertEquals(2, newDeskLookup.length);
      assertTrue(areAcceleratorsEqual(
          overridenAccel,
          newDeskLookup[1]!.layoutProperties.defaultAccelerator.accelerator));

      // Replacing a default accelerator should not remove it but rather disable
      // it.
      const snapWindowRightLookup = getManager().getAcceleratorInfos(
          AcceleratorSource.kAsh, snapWindowRightAction);
      assertEquals(1, snapWindowRightLookup.length);
      assertEquals(
          AcceleratorState.kDisabledByUser, snapWindowRightLookup[0]!.state);
    });
  });

  test('RemoveDefaultAccelerator', () => {
    getProvider().setFakeAcceleratorConfig(fakeAcceleratorConfig);
    return getProvider().getAccelerators().then((result) => {
      assertDeepEquals(fakeAcceleratorConfig, result.config);

      getManager().setAcceleratorLookup(result.config);

      // Get Snap Window Right accelerator from kAsh[1].
      const expectedAction = 1;

      // Initially there is only one accelerator for Snap Window Right.
      const lookup = getManager().getAcceleratorInfos(
          AcceleratorSource.kAsh, expectedAction);
      assertEquals(1, lookup.length);

      // Remove the accelerator.
      const removedAccelerator =
          lookup[0]!.layoutProperties.defaultAccelerator.accelerator;
      getManager().removeAccelerator(
          AcceleratorSource.kAsh, expectedAction, removedAccelerator);

      // Removing a default accelerator only disables it.
      assertEquals(1, lookup.length);
      assertEquals(AcceleratorState.kDisabledByUser, lookup[0]!.state);

      // Removed accelerator should not appear in the reverse lookup.
      assertEquals(
          undefined,
          getManager().getAcceleratorIdFromReverseLookup(removedAccelerator));
    });
  });

  test('AddAndRemoveAccelerator', () => {
    getProvider().setFakeAcceleratorConfig(fakeAcceleratorConfig);
    return getProvider().getAccelerators().then((result) => {
      assertDeepEquals(fakeAcceleratorConfig, result.config);

      getManager().setAcceleratorLookup(result.config);

      // Get Snap Window Right accelerator from kAsh[1]!.
      const expectedAction = 1;

      // Initially there is only one accelerator for Snap Window Right.
      const lookup = getManager().getAcceleratorInfos(
          AcceleratorSource.kAsh, expectedAction);
      assertEquals(1, lookup.length);

      const expectedNewAccel: Accelerator = {
        modifiers: Modifier.CONTROL,
        keyCode: 79,
      };

      // Sanity check that new accel is not in the reverse lookup.
      assertEquals(
          undefined,
          getManager().getAcceleratorIdFromReverseLookup(expectedNewAccel));

      addAndVerify(
          AcceleratorSource.kAsh, expectedAction,
          createEmptyAccelInfoFromAccel(expectedNewAccel));

      // Check that the accelerator got updated in the lookup.
      assertEquals(2, lookup.length);
      assertTrue(areAcceleratorsEqual(
          expectedNewAccel,
          lookup[1]!.layoutProperties.defaultAccelerator.accelerator));

      // Remove the accelerator.
      const removedAccelerator =
          lookup[1]!.layoutProperties.defaultAccelerator.accelerator;
      getManager().removeAccelerator(
          AcceleratorSource.kAsh, expectedAction, removedAccelerator);

      // Expect only 1 accelerator.
      assertEquals(1, lookup.length);

      // Removed accelerator should not appear in the reverse lookup.
      assertEquals(
          undefined,
          getManager().getAcceleratorIdFromReverseLookup(removedAccelerator));
    });
  });
});