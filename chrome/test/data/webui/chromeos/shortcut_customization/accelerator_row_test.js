// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AcceleratorRowElement} from 'chrome://shortcut-customization/accelerator_row.js';
import {AcceleratorInfo, AcceleratorKeys, AcceleratorSource, AcceleratorState, AcceleratorType, Modifier} from 'chrome://shortcut-customization/shortcut_types.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

import {CreateUserAccelerator} from './shortcut_customization_test_util.js';

export function acceleratorRowTest() {
  /** @type {?AcceleratorRowElement} */
  let rowElement = null;

  setup(() => {
    rowElement = /** @type {!AcceleratorRowElement} */ (
        document.createElement('accelerator-row'));
    document.body.appendChild(rowElement);
  });

  teardown(() => {
    rowElement.remove();
    rowElement = null;
  });

  test('LoadsBasicRow', async () => {
    const acceleratorInfo1 = CreateUserAccelerator(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*key_display=*/ 'g');

    /** @type {!AcceleratorInfo} */
    const acceleratorInfo2 = CreateUserAccelerator(
        Modifier.CONTROL,
        /*key=*/ 67,
        /*key_display=*/ 'c');

    const accelerators = [acceleratorInfo1, acceleratorInfo2];
    const description = 'test shortcut';

    rowElement.acceleratorInfos = accelerators;
    rowElement.description = description;
    await flush();
    const acceleratorElements =
        rowElement.shadowRoot.querySelectorAll('accelerator-view');
    assertEquals(2, acceleratorElements.length);
    assertEquals(
        description,
        rowElement.shadowRoot.querySelector('#descriptionText')
            .textContent.trim());

    const keys1 =
        acceleratorElements[0].shadowRoot.querySelectorAll('input-key');
    // SHIFT + CONTROL + g
    assertEquals(3, keys1.length);
    assertEquals(
        'shift', keys1[0].shadowRoot.querySelector('#key').textContent.trim());
    assertEquals(
        'ctrl', keys1[1].shadowRoot.querySelector('#key').textContent.trim());
    assertEquals(
        'g', keys1[2].shadowRoot.querySelector('#key').textContent.trim());

    const keys2 =
        acceleratorElements[1].shadowRoot.querySelectorAll('input-key');
    // CONTROL + c
    assertEquals(2, keys2.length);
    assertEquals(
        'ctrl', keys2[0].shadowRoot.querySelector('#key').textContent.trim());
    assertEquals(
        'c', keys2[1].shadowRoot.querySelector('#key').textContent.trim());
  });

  test('LockIcon', async () => {
    const acceleratorInfo1 = CreateUserAccelerator(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*key_display=*/ 'g');

    const accelerators = [acceleratorInfo1];
    const description = 'test shortcut';

    rowElement.acceleratorInfos = accelerators;
    rowElement.description = description;
    rowElement.source = AcceleratorSource.kBrowser;
    await flushTasks();

    // Expected the lock icon to appear if the source is kBrowser.
    assertFalse(
        rowElement.shadowRoot.querySelector('#lockIconContainer').hidden);

    // Update source to be kAsh, lock icon should no longer appear.
    rowElement.source = AcceleratorSource.kAsh;
    await flushTasks();
    assertTrue(
        rowElement.shadowRoot.querySelector('#lockIconContainer').hidden);
  });
}
