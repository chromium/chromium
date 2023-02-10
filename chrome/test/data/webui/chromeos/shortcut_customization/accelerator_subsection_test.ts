// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shortcut-customization/js/accelerator_subsection.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AcceleratorLookupManager} from 'chrome://shortcut-customization/js/accelerator_lookup_manager.js';
import {AcceleratorSubsectionElement} from 'chrome://shortcut-customization/js/accelerator_subsection.js';
import {fakeAcceleratorConfig, fakeLayoutInfo} from 'chrome://shortcut-customization/js/fake_data.js';
import {AcceleratorCategory, AcceleratorSource, AcceleratorSubcategory, LayoutInfo, LayoutStyle, Modifier} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {createUserAcceleratorInfo} from './shortcut_customization_test_util.js';

suite('acceleratorSubsectionTest', function() {
  let sectionElement: AcceleratorSubsectionElement|null = null;

  let manager: AcceleratorLookupManager|null = null;

  setup(() => {
    manager = AcceleratorLookupManager.getInstance();
    manager!.setAcceleratorLookup(fakeAcceleratorConfig);
    manager!.setAcceleratorLayoutLookup(fakeLayoutInfo);

    sectionElement = document.createElement('accelerator-subsection');
    document.body.appendChild(sectionElement);
  });

  teardown(() => {
    if (manager) {
      manager!.reset();
    }
    sectionElement!.remove();
    sectionElement = null;
  });

  // TODO(jimmyxgong): Update this test after retrieving accelerators is
  // implemented for a subsection.
  test('LoadsBasicSection', async () => {
    const acceleratorInfo1 = createUserAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');

    const acceleratorInfo2 = createUserAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 67,
        /*keyDisplay=*/ 'c');

    const expectedAccelInfos = [acceleratorInfo1, acceleratorInfo2];
    const description = 'test shortcut';
    const title = 'test title';
    const expectedLayoutInfo: LayoutInfo = {
      action: 0,
      category: AcceleratorCategory.kWindowsAndDesks,
      description,
      source: AcceleratorSource.kAsh,
      style: LayoutStyle.kDefault,
      subCategory: AcceleratorSubcategory.kWindows,
    };

    sectionElement!.title = title;
    sectionElement!.accelRowDataArray = [{
      acceleratorInfos: expectedAccelInfos,
      layoutInfo: expectedLayoutInfo,
    }];

    await flush();
    assertEquals(
        title,
        sectionElement!.shadowRoot!.querySelector(
                                       '#title')!.textContent!.trim());
  });

  test('LoadCategoryAndConfirmDescriptions', async () => {
    const expectedTitle = 'test title';
    sectionElement!.title = expectedTitle;
    sectionElement!.category = AcceleratorCategory.kWindowsAndDesks;
    sectionElement!.subcategory = AcceleratorSubcategory.kWindows;

    await flushTasks();

    const rowListElement =
        sectionElement!.shadowRoot!.querySelectorAll('accelerator-row');

    // First accelerator-row corresponds to 'Snap Window Left'.
    assertEquals(
        manager!.getAcceleratorName(/*source=*/ 0, /*action=*/ 0)!,
        rowListElement[0]!.description);
    // Second accelerator-row corresponds to 'Snap Window Right'.
    assertEquals(
        manager!.getAcceleratorName(/*source=*/ 0, /*action=*/ 1)!,
        rowListElement[1]!.description);
  });
});
