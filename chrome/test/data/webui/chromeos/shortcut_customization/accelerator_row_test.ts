// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shortcut-customization/js/accelerator_row.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AcceleratorRowElement} from 'chrome://shortcut-customization/js/accelerator_row.js';
import {InputKeyElement} from 'chrome://shortcut-customization/js/input_key.js';
import {AcceleratorSource, Modifier} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/test_util.js';

import {createUserAccelerator} from './shortcut_customization_test_util.js';

suite('acceleratorRowTest', function() {
  let rowElement: AcceleratorRowElement|null = null;

  setup(() => {
    rowElement = document.createElement('accelerator-row');
    document.body.appendChild(rowElement);
  });

  teardown(() => {
    if (rowElement) {
      rowElement.remove();
    }
    rowElement = null;
  });

  //   function getInputKey() {

  //   }

  test('LoadsBasicRow', async () => {
    const acceleratorInfo1 = createUserAccelerator(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');

    const acceleratorInfo2 = createUserAccelerator(
        Modifier.CONTROL,
        /*key=*/ 67,
        /*keyDisplay=*/ 'c');

    const accelerators = [acceleratorInfo1, acceleratorInfo2];
    const description = 'test shortcut';

    rowElement!.acceleratorInfos = accelerators;
    rowElement!.description = description;
    await flush();
    const acceleratorElements =
        rowElement!.shadowRoot!.querySelectorAll('accelerator-view');
    assertEquals(2, acceleratorElements.length);
    assertEquals(
        description,
        rowElement!.shadowRoot!.querySelector(
                                   '#descriptionText')!.textContent!.trim());

    const keys1: NodeListOf<InputKeyElement> =
        acceleratorElements[0]!.shadowRoot!.querySelectorAll('input-key');
    // SHIFT + CONTROL + g
    assertEquals(3, keys1.length);
    assertEquals(
        'shift',
        keys1[0]!.shadowRoot!.querySelector('#key')!.textContent!.trim());
    assertEquals(
        'ctrl',
        keys1[1]!.shadowRoot!.querySelector('#key')!.textContent!.trim());
    assertEquals(
        'g', keys1[2]!.shadowRoot!.querySelector('#key')!.textContent!.trim());

    const keys2 =
        acceleratorElements[1]!.shadowRoot!.querySelectorAll('input-key');
    // CONTROL + c
    assertEquals(2, keys2.length);
    assertEquals(
        'ctrl',
        keys2[0]!.shadowRoot!.querySelector('#key')!.textContent!.trim());
    assertEquals(
        'c', keys2[1]!.shadowRoot!.querySelector('#key')!.textContent!.trim());
  });

  test('LockIcon', async () => {
    const acceleratorInfo1 = createUserAccelerator(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');

    const accelerators = [acceleratorInfo1];
    const description = 'test shortcut';

    rowElement!.acceleratorInfos = accelerators;
    rowElement!.description = description;
    rowElement!.source = AcceleratorSource.BROWSER;
    await flushTasks();

    // Expected the lock icon to appear if the source is kBrowser.
    let lockItemContainer = rowElement!.shadowRoot!.querySelector(
                                '#lockIconContainer') as HTMLDivElement;
    assertFalse(lockItemContainer.hidden);

    // Update source to be kAsh, lock icon should no longer appear.
    rowElement!.source = AcceleratorSource.ASH;
    await flushTasks();
    lockItemContainer = rowElement!.shadowRoot!.querySelector(
                            '#lockIconContainer') as HTMLDivElement;

    assertTrue(lockItemContainer.hidden);
  });
});
