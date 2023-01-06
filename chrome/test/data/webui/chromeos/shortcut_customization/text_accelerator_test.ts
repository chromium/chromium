// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shortcut-customization/js/text_accelerator.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {TextAcceleratorElement} from 'chrome://shortcut-customization/js/text_accelerator.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';


export function initTextAcceleratorElement(): TextAcceleratorElement {
  const element = document.createElement('text-accelerator');
  document.body.appendChild(element);
  flush();
  return element;
}

suite('textAcceleratorTest', function() {
  let textAccelElement: TextAcceleratorElement|null = null;

  teardown(() => {
    if (textAccelElement) {
      textAccelElement.remove();
    }
    textAccelElement = null;
  });

  test('InitializeTextAcceleratorElement', async () => {
    // TODO(michaelcheco): Remove stub test.
    textAccelElement = initTextAcceleratorElement();
    assertTrue(!!textAccelElement);
  });
});
