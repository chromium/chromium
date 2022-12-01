// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shortcut-customization/js/input_key.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {IronIconElement} from '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {InputKeyElement} from 'chrome://shortcut-customization/js/input_key.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

function initInputKeyElement(): InputKeyElement {
  const element = document.createElement('input-key');
  document.body.appendChild(element);
  flush();
  return element;
}

suite('inputKeyTest', function() {
  let inputKeyElement: InputKeyElement|null = null;

  teardown(() => {
    if (inputKeyElement) {
      inputKeyElement.remove();
    }
    inputKeyElement = null;
  });

  test('BasicKeys', async () => {
    inputKeyElement = initInputKeyElement();
    inputKeyElement.key = 'a';
    await flush();

    const keyElement = inputKeyElement.shadowRoot!.querySelector('#key-text');
    assertTrue(!!keyElement);
    assertTrue(isVisible(keyElement));
    assertEquals('a', keyElement.textContent);

    const iconElement = inputKeyElement.shadowRoot!.querySelector('#key-icon');
    assertFalse(isVisible(iconElement));
  });

  test('IconKeys', async () => {
    inputKeyElement = initInputKeyElement();
    inputKeyElement.key = 'PrintScreen';
    await flush();

    const iconElement = inputKeyElement.shadowRoot!.querySelector(
                            '#key-icon') as IronIconElement;
    assertTrue(isVisible(iconElement));
    assertEquals('shortcut-customization-keys:screenshot', iconElement.icon);

    const keyElement = inputKeyElement.shadowRoot!.querySelector('#key-text');
    assertFalse(isVisible(keyElement));
  });
});