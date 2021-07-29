// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AcceleratorSubsectionElement} from 'chrome://shortcut-customization/accelerator_subsection.js';
import {Modifier} from 'chrome://shortcut-customization/shortcut_types.js';

import {assertEquals} from '../../chai_assert.js';

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
    // TODO(jimmyxgong): Update the type of the test accelerator with the mojom
    // version.
    const accelerators = [
      {modifiers: Modifier.SHIFT | Modifier.CONTROL, key: 'g', rawKey: 0x0},
      {modifiers: Modifier.CONTROL, key: 'c', rawKey: 0x0}
    ];
    const description = 'test shortcut';
    const title = 'test title';

    sectionElement.title = title;
    sectionElement.acceleratorContainer = [{
      description: description,
      accelerators: accelerators,
    }];
    await flush();
    assertEquals(
        title,
        sectionElement.shadowRoot.querySelector('#title').textContent.trim());
  });
}