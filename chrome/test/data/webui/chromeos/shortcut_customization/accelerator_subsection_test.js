// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AcceleratorSubsectionElement} from 'chrome://shortcut-customization/accelerator_subsection.js';
import {AcceleratorInfo, AcceleratorKeys, AcceleratorState, AcceleratorType, Modifier} from 'chrome://shortcut-customization/shortcut_types.js';

import {assertEquals} from '../../chai_assert.js';

import {CreateDefaultAccelerator} from './shortcut_customization_test_util.js';

export function acceleratorSubsectionTest() {
  /** @type {?AcceleratorSubsectionElement} */
  let sectionElement = null;

  setup(() => {
    sectionElement = /** @type {!AcceleratorSubsectionElement} */ (
        document.createElement('accelerator-subsection'));
    document.body.appendChild(sectionElement);
  });

  teardown(() => {
    sectionElement.remove();
    sectionElement = null;
  });

  // TODO(jimmyxgong): Update this test after retrieving accelerators is
  // implemented for a subsection.
  test('LoadsBasicSection', async () => {
    /** @type {!AcceleratorInfo} */
    const acceleratorInfo1 = CreateDefaultAccelerator(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*key_display=*/ 'g');

    /** @type {!AcceleratorInfo} */
    const acceleratorInfo2 = CreateDefaultAccelerator(
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
}