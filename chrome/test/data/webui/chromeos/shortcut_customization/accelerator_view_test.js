// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AcceleratorLookupManager} from 'chrome://shortcut-customization/accelerator_lookup_manager.js';
import {AcceleratorViewElement, ViewState} from 'chrome://shortcut-customization/accelerator_view.js';
import {fakeAcceleratorConfig, fakeLayoutInfo} from 'chrome://shortcut-customization/fake_data.js';
import {AcceleratorInfo, AcceleratorKeys, AcceleratorSource, AcceleratorState, AcceleratorType, Modifier} from 'chrome://shortcut-customization/shortcut_types.js';

import {assertEquals, assertTrue} from '../../chai_assert.js';

import {CreateDefaultAccelerator, CreateUserAccelerator} from './shortcut_customization_test_util.js';

export function acceleratorViewTest() {
  /** @type {?AcceleratorViewElement} */
  let viewElement = null;

  /** @type {?AcceleratorLookupManager} */
  let manager = null;

  setup(() => {
    manager = AcceleratorLookupManager.getInstance();
    manager.setAcceleratorLookup(fakeAcceleratorConfig);
    manager.setAcceleratorLayoutLookup(fakeLayoutInfo);

    viewElement = /** @type {!AcceleratorViewElement} */ (
        document.createElement('accelerator-view'));
    document.body.appendChild(viewElement);
  });

  teardown(() => {
    manager.reset();

    viewElement.remove();
    viewElement = null;
  });

  test('LoadsBasicAccelerator', async () => {
    /** @type {!AcceleratorInfo} */
    const acceleratorInfo = CreateUserAccelerator(
        Modifier.CONTROL | Modifier.SHIFT,
        /*key=*/ 71,
        /*key_display=*/ 'g');


    viewElement.acceleratorInfo = acceleratorInfo;
    await flush();
    const keys = viewElement.shadowRoot.querySelectorAll('input-key');
    // Three keys: shift, control, g
    assertEquals(3, keys.length);

    assertEquals(
        'shift', keys[0].shadowRoot.querySelector('#key').textContent.trim());
    assertEquals(
        'ctrl', keys[1].shadowRoot.querySelector('#key').textContent.trim());
    assertEquals(
        'g', keys[2].shadowRoot.querySelector('#key').textContent.trim());
  });

  test('EditableAccelerator', async () => {
    /** @type {!AcceleratorInfo} */
    const acceleratorInfo = CreateDefaultAccelerator(
        Modifier.ALT,
        /*key=*/ 221,
        /*key_display=*/ ']');

    viewElement.acceleratorInfo = acceleratorInfo;
    viewElement.source = AcceleratorSource.kAsh;
    viewElement.action = 1;
    await flush();
    // Enable the edit view.
    viewElement.viewState = ViewState.EDIT;

    await flush();

    const ctrlKey = viewElement.shadowRoot.querySelector('#ctrlKey');
    const altKey = viewElement.shadowRoot.querySelector('#altKey');
    const shiftKey = viewElement.shadowRoot.querySelector('#shiftKey');
    const metaKey = viewElement.shadowRoot.querySelector('#searchKey');
    const pendingKey = viewElement.shadowRoot.querySelector('#pendingKey');

    // By default, no keys should be registered.
    assertEquals('not-selected', ctrlKey.keyState);
    assertEquals('not-selected', altKey.keyState);
    assertEquals('not-selected', shiftKey.keyState);
    assertEquals('not-selected', metaKey.keyState);
    assertEquals('not-selected', pendingKey.keyState);
    assertEquals('key', pendingKey.key);

    // Simulate Ctrl + Alt + e.
    viewElement.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'e',
      keyCode: '69',
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
}