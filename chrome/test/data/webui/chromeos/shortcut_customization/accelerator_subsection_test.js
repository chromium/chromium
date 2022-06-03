// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AcceleratorLookupManager} from 'chrome://shortcut-customization/accelerator_lookup_manager.js';
import {AcceleratorSubsectionElement} from 'chrome://shortcut-customization/accelerator_subsection.js';
import {fakeAcceleratorConfig, fakeLayoutInfo} from 'chrome://shortcut-customization/fake_data.js';
import {AcceleratorInfo, AcceleratorKeys, AcceleratorState, AcceleratorType, Modifier} from 'chrome://shortcut-customization/shortcut_types.js';

import {assertEquals} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

import {CreateUserAccelerator} from './shortcut_customization_test_util.js';

export function acceleratorSubsectionTest() {
  /** @type {?AcceleratorSubsectionElement} */
  let sectionElement = null;

  /** @type {?AcceleratorLookupManager} */
  let manager = null;

  setup(() => {
    manager = AcceleratorLookupManager.getInstance();
    manager.setAcceleratorLookup(fakeAcceleratorConfig);
    manager.setAcceleratorLayoutLookup(fakeLayoutInfo);

    sectionElement = /** @type {!AcceleratorSubsectionElement} */ (
        document.createElement('accelerator-subsection'));
    document.body.appendChild(sectionElement);
  });

  teardown(() => {
    manager.reset();
    sectionElement.remove();
    sectionElement = null;
  });

  // TODO(jimmyxgong): Update this test after retrieving accelerators is
  // implemented for a subsection.
  test('LoadsBasicSection', async () => {
    /** @type {!AcceleratorInfo} */
    const acceleratorInfo1 = CreateUserAccelerator(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*key_display=*/ 'g');

    /** @type {!AcceleratorInfo} */
    const acceleratorInfo2 = CreateUserAccelerator(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 67,
        /*key_display=*/ 'c');

    const accelerators = [acceleratorInfo1, acceleratorInfo2];
    const description = 'test shortcut';
    const title = 'test title';

    sectionElement.title = title;
    sectionElement.acceleratorContainer = [{
      description: description,
      acceleratorInfos: accelerators,
    }];

    await flush();
    assertEquals(
        title,
        sectionElement.shadowRoot.querySelector('#title').textContent.trim());
  });

  test('LoadCategoryAndConfirmDescriptions', async () => {
    const expectedTitle = 'test title';
    sectionElement.title = expectedTitle;
    sectionElement.category = /*Chromeos*/ 0;
    sectionElement.subcategory = /*Window Management*/ 0;

    await flushTasks();

    const rowListElement =
        sectionElement.shadowRoot.querySelectorAll('accelerator-row');

    // First accelerator-row corresponds to 'Snap Window Left'.
    assertEquals(
        manager.getAcceleratorName(/*source=*/ 0, /*action=*/ 0),
        rowListElement[0].description);
    // Second accelerator-row corresponds to 'Snap Window Right'.
    assertEquals(
        manager.getAcceleratorName(/*source=*/ 0, /*action=*/ 1),
        rowListElement[1].description);
  });
}
