// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shortcut-customization/js/text_accelerator.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {TextAcceleratorElement} from 'chrome://shortcut-customization/js/text_accelerator.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';


export function initTextAcceleratorElement(): TextAcceleratorElement {
  const element = document.createElement('text-accelerator');
  document.body.appendChild(element);
  flush();
  return element;
}

suite('acceleratorRowTest', function() {
  let textAccelElement: TextAcceleratorElement|null = null;

  teardown(() => {
    if (textAccelElement) {
      textAccelElement.remove();
    }
    textAccelElement = null;
  });

  test('BasicTextDisplay', async () => {
    textAccelElement = initTextAcceleratorElement();
    const expectedText = 'string test';
    textAccelElement.text = expectedText;
    flush();

    const textWrapper = textAccelElement!.shadowRoot!.querySelector(
                            '#text-wrapper') as HTMLDivElement;
    assertTrue(!!textWrapper);
    assertEquals(expectedText, textWrapper.innerText);
  });

  test('HtmlTextDisplay', async () => {
    textAccelElement = initTextAcceleratorElement();
    const expectedText = 'html test';
    const expectedHtml = `<div>${expectedText}</div>`;
    textAccelElement.text = expectedHtml;
    flush();

    const textWrapper = textAccelElement!.shadowRoot!.querySelector(
                            '#text-wrapper') as HTMLDivElement;
    assertTrue(!!textWrapper);
    assertEquals(expectedText, textWrapper.innerText);
    assertEquals(expectedHtml, textWrapper.innerHTML);
  });
});
