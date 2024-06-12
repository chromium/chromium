// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {AcceleratorLookupManager} from 'chrome://shortcut-customization/js/accelerator_lookup_manager.js';
import {fakeAcceleratorConfig, fakeAmbientConfig, fakeLayoutInfo} from 'chrome://shortcut-customization/js/fake_data.js';
import {FakeShortcutProvider} from 'chrome://shortcut-customization/js/fake_shortcut_provider.js';
import {AcceleratorCategory, AcceleratorSource, AcceleratorSubcategory, MetaKey} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

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
    });
  });

  test('GetIsSubcategoryLocked', async () => {
    // First, initialize the accelerators into the AcceleratorLookupManager.
    getProvider().setFakeAcceleratorConfig(fakeAcceleratorConfig);
    const {config: accelConfig} = await getProvider().getAccelerators();
    assertDeepEquals(fakeAcceleratorConfig, accelConfig);
    getManager().setAcceleratorLookup(accelConfig);

    // Then, initialize the layout infos into the AcceleratorLookupManager.
    getProvider().setFakeAcceleratorLayoutInfos(fakeLayoutInfo);
    const {layoutInfos: layoutInfos} =
        await getProvider().getAcceleratorLayoutInfos();
    assertDeepEquals(fakeLayoutInfo, layoutInfos);
    getManager().setAcceleratorLayoutLookup(layoutInfos);

    // We expect that kWindows subcategory is not locked.
    assertFalse(
        getManager().isSubcategoryLocked(AcceleratorSubcategory.kWindows));
    // We expect that kTabs subcategory is locked.
    assertTrue(getManager().isSubcategoryLocked(AcceleratorSubcategory.kTabs));
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

  test('SetAndGetMetaKeyToDisplay', () => {
    getProvider().setFakeMetaKeyToDisplay(MetaKey.kLauncher);
    return getProvider().getMetaKeyToDisplay().then(
        (result: {metaKey: MetaKey}) => {
          assertEquals(MetaKey.kLauncher, result.metaKey);

          getManager().setMetaKeyToDisplay(result.metaKey);
          assertEquals(MetaKey.kLauncher, getManager().getMetaKeyToDisplay());
        });
  });
});
