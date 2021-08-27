// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AcceleratorLookupManager} from 'chrome://shortcut-customization/accelerator_lookup_manager.js';
import {fakeAcceleratorConfig, fakeLayoutInfo} from 'chrome://shortcut-customization/fake_data.js';
import {FakeShortcutProvider} from 'chrome://shortcut-customization/fake_shortcut_provider.js';
import {AcceleratorConfig, AcceleratorInfo, AcceleratorSource, LayoutInfoList,} from 'chrome://shortcut-customization/shortcut_types.js';

import {assertDeepEquals, assertEquals} from '../../chai_assert.js';

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
}