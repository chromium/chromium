// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {AcceleratorLookupManager} from 'chrome://shortcut-customization/js/accelerator_lookup_manager.js';
import {fakeAcceleratorConfig, fakeAmbientConfig, fakeLayoutInfo} from 'chrome://shortcut-customization/js/fake_data.js';
import {FakeShortcutProvider} from 'chrome://shortcut-customization/js/fake_shortcut_provider.js';
import {Accelerator, AcceleratorCategory, AcceleratorSource, AcceleratorState, Modifier, MojoAccelerator, MojoAcceleratorInfo, StandardAcceleratorInfo} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {areAcceleratorsEqual, createEmptyAccelInfoFromAccel} from 'chrome://shortcut-customization/js/shortcut_utils.js';
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
      source: AcceleratorSource, action: number, oldAccel: Accelerator,
      newAccelInfo: StandardAcceleratorInfo) {
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
            newAccelInfo.layoutProperties.standardAccelerator.accelerator));
  }

  function addAndVerify(
      source: AcceleratorSource, action: number,
      newAccelInfo: StandardAcceleratorInfo) {
    getManager().addAccelerator(source, action, newAccelInfo);

    // Verify that the new accelerator is in the reverse lookup.
    assertEquals(
        `${source}-${action}`,
        getManager().getAcceleratorIdFromReverseLookup(
            newAccelInfo.layoutProperties.standardAccelerator.accelerator));
  }

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

      // We expect 2 subcategories for kWindowsAndDesks: kWindows and
      // kDesks.
      assertEquals(
          2,
          getManager().getSubcategories(
                          AcceleratorCategory.kWindowsAndDesks)!.size);
      // We expect 1 subcategory for kBrowser: kTabs.
      assertEquals(
          1, getManager().getSubcategories(AcceleratorCategory.kBrowser)!.size);

      // We expect 1 subcategory for kEventRewriter: kSixPackKeys.
      assertEquals(
          1,
          getManager().getSubcategories(
                          AcceleratorCategory.kEventRewriter)!.size);
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
                           .standardAccelerator!.accelerator;

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
      let lookup = getManager().getStandardAcceleratorInfos(
          AcceleratorSource.kAsh, expectedAction);
      // Replacing a default shortcut should not remove the default. Expect
      // a new accelerator to be added instead.
      assertEquals(2, lookup.length);
      assertTrue(areAcceleratorsEqual(
          expectedNewAccel,
          lookup[1]!.layoutProperties.standardAccelerator.accelerator));

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
      lookup = getManager().getStandardAcceleratorInfos(
          AcceleratorSource.kAsh, expectedAction);
      // Expect only one accelerator since the previous accelerator has been
      // removed but the default accelerator has been re-enabled.
      assertEquals(1, lookup.length);
      assertTrue(areAcceleratorsEqual(
          expectedNewDefaultAccel,
          lookup[0]!.layoutProperties.standardAccelerator.accelerator));
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
                                 .standardAccelerator!.accelerator;

      // Replace New Desk shortcut with Alt+']'.
      const newDeskAction = 2;
      const oldNewDeskAccels = ashMap![newDeskAction];
      const oldNewDeskAccel = oldNewDeskAccels![0]!.layoutProperties!
                                  .standardAccelerator!.accelerator;

      replaceAndVerify(
          AcceleratorSource.kAsh, newDeskAction, oldNewDeskAccel,
          createEmptyAccelInfoFromAccel(overridenAccel));

      // Verify that the New Desk shortcut now has the ALT + ']' accelerator.
      const newDeskLookup = getManager().getStandardAcceleratorInfos(
          AcceleratorSource.kAsh, newDeskAction);
      assertEquals(2, newDeskLookup.length);
      assertTrue(areAcceleratorsEqual(
          overridenAccel,
          newDeskLookup[1]!.layoutProperties.standardAccelerator.accelerator));

      // There should still be 1 accelerator for snapWindowRight, but the
      // default should be disabled.
      const snapWindowRightLookup = getManager().getStandardAcceleratorInfos(
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
      const lookup = getManager().getStandardAcceleratorInfos(
          AcceleratorSource.kAsh, expectedAction);
      assertEquals(2, lookup.length);
      assertTrue(areAcceleratorsEqual(
          expectedNewAccel,
          lookup[1]!.layoutProperties.standardAccelerator.accelerator));
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
          snapWindowRightAccels[0]!.layoutProperties!.standardAccelerator!
              .accelerator as MojoAccelerator;

      // Replace New Desk shortcut with Alt+']'.
      const newDeskAction = 2;

      addAndVerify(
          AcceleratorSource.kAsh, newDeskAction,
          createEmptyAccelInfoFromAccel(overridenAccel));

      // Verify that the New Desk shortcut now has the ALT + ']' accelerator.
      const newDeskLookup = getManager().getStandardAcceleratorInfos(
          AcceleratorSource.kAsh, newDeskAction);
      assertEquals(2, newDeskLookup.length);
      assertTrue(areAcceleratorsEqual(
          overridenAccel,
          newDeskLookup[1]!.layoutProperties.standardAccelerator.accelerator));

      // Replacing a default accelerator should not remove it but rather disable
      // it.
      const snapWindowRightLookup = getManager().getStandardAcceleratorInfos(
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
      const lookup = getManager().getStandardAcceleratorInfos(
          AcceleratorSource.kAsh, expectedAction);
      assertEquals(1, lookup.length);

      // Remove the accelerator.
      const removedAccelerator =
          lookup[0]!.layoutProperties.standardAccelerator.accelerator;
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
      const lookup = getManager().getStandardAcceleratorInfos(
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
          lookup[1]!.layoutProperties.standardAccelerator.accelerator));

      // Remove the accelerator.
      const removedAccelerator =
          lookup[1]!.layoutProperties.standardAccelerator.accelerator;
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

  test('AcceleratorsAddedToCorrectLookupMap', () => {
    getProvider().setFakeAcceleratorConfig(fakeAmbientConfig);
    return getProvider().getAccelerators().then((result) => {
      assertDeepEquals(fakeAmbientConfig, result.config);

      getManager().setAcceleratorLookup(result.config);
      // New tab accelerator from kAmbient[0]!.
      const expectedNewTabAction = 0;
      // Cycle tabs accelerator from kAmbient[1]!.
      const expectedCycleTabsAction = 1;

      const standardLookup = getManager().getStandardAcceleratorInfos(
          AcceleratorSource.kAmbient, expectedNewTabAction);
      assertEquals(1, standardLookup.length);

      const textLookup = getManager().getTextAcceleratorInfos(
          AcceleratorSource.kAmbient, expectedCycleTabsAction);
      assertEquals(1, textLookup.length);
    });
  });
});