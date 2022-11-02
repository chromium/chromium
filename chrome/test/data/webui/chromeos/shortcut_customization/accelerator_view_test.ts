// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shortcut-customization/js/accelerator_view.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AcceleratorLookupManager} from 'chrome://shortcut-customization/js/accelerator_lookup_manager.js';
import {AcceleratorViewElement, ViewState} from 'chrome://shortcut-customization/js/accelerator_view.js';
import {fakeAcceleratorConfig, fakeLayoutInfo} from 'chrome://shortcut-customization/js/fake_data.js';
import {InputKeyElement, KeyInputState} from 'chrome://shortcut-customization/js/input_key.js';
import {AcceleratorSource, Modifier} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {createDefaultAcceleratorInfo, createUserAcceleratorInfo} from './shortcut_customization_test_util.js';

suite('acceleratorViewTest', function() {
  let viewElement: AcceleratorViewElement|null = null;

  let manager: AcceleratorLookupManager|null = null;

  setup(() => {
    manager = AcceleratorLookupManager.getInstance();
    manager.setAcceleratorLookup(fakeAcceleratorConfig);
    manager.setAcceleratorLayoutLookup(fakeLayoutInfo);

    viewElement = document.createElement('accelerator-view');
    document.body.appendChild(viewElement);
  });

  teardown(() => {
    if (manager) {
      manager.reset();
    }

    viewElement!.remove();
    viewElement = null;
  });

  function getInputKey(selector: string): InputKeyElement {
    const element = viewElement!.shadowRoot!.querySelector(selector);
    assertTrue(!!element);
    return element as InputKeyElement;
  }

  test('LoadsBasicAccelerator', async () => {
    const acceleratorInfo = createUserAcceleratorInfo(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*keyDisplay=*/ 'g');


    viewElement!.acceleratorInfo = acceleratorInfo;
    await flush();
    const keys = viewElement!.shadowRoot!.querySelectorAll('input-key');
    // Three keys: shift, control, g
    assertEquals(3, keys.length);

    assertEquals(
        'shift',
        keys[0]!.shadowRoot!.querySelector('#key')!.textContent!.trim());
    assertEquals(
        'ctrl',
        keys[1]!.shadowRoot!.querySelector('#key')!.textContent!.trim());
    assertEquals(
        'g', keys[2]!.shadowRoot!.querySelector('#key')!.textContent!.trim());
  });

  test('EditableAccelerator', async () => {
    const acceleratorInfo = createDefaultAcceleratorInfo(
        Modifier.ALT,
        /*key=*/ 221,
        /*keyDisplay=*/ ']');

    viewElement!.acceleratorInfo = acceleratorInfo;
    viewElement!.source = AcceleratorSource.kAsh;
    viewElement!.action = 1;
    await flush();
    // Enable the edit view.
    viewElement!.viewState = ViewState.EDIT;

    await flush();

    const ctrlKey = getInputKey('#ctrlKey');
    const altKey = getInputKey('#altKey');
    const shiftKey = getInputKey('#shiftKey');
    const metaKey = getInputKey('#searchKey');
    const pendingKey = getInputKey('#pendingKey');

    // By default, no keys should be registered.
    assertEquals(KeyInputState.NOT_SELECTED, ctrlKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, altKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, shiftKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, metaKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, pendingKey.keyState);
    assertEquals('key', pendingKey.key);

    // Simulate Ctrl + Alt + e.
    viewElement!.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'e',
      keyCode: 69,
      code: 'KeyE',
      ctrlKey: true,
      altKey: true,
      shiftKey: false,
      metaKey: false,
    }));

    await flush();

    assertEquals('modifier-selected', ctrlKey.keyState);
    assertEquals('modifier-selected', altKey.keyState);
    assertEquals('not-selected', shiftKey.keyState);
    assertEquals('not-selected', metaKey.keyState);
    assertEquals('alpha-numeric-selected', pendingKey.keyState);
    assertEquals('e', pendingKey.key);
  });
});