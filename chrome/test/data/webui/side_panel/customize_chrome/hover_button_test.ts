// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://customize-chrome-side-panel.top-chrome/hover_button.js';

import {HoverButtonElement} from 'chrome://customize-chrome-side-panel.top-chrome/hover_button.js';
import {listenOnce} from 'chrome://resources/js/util_ts.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('HoverButtonTest', () => {
  let hoverButtonElement: HoverButtonElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    hoverButtonElement =
        document.createElement('customize-chrome-hover-button');
    document.body.appendChild(hoverButtonElement);
  });

  test(
      'setting `label` gives `customize-chrome-button-label` a label',
      async () => {
        // Act.
        hoverButtonElement.label = 'foo';

        // Assert.
        const buttonLabel = hoverButtonElement.shadowRoot!.querySelector(
            'customize-chrome-button-label');
        assertEquals(hoverButtonElement.label, buttonLabel!.label);
        assertEquals(null, hoverButtonElement.labelDescription);
        assertEquals(
            hoverButtonElement.labelDescription, buttonLabel!.labelDescription);
      });

  test(
      'setting `labelDescription` gives `customize-chrome-button-label`' +
          'a label description',
      async () => {
        // Act.
        hoverButtonElement.label = 'foo';
        hoverButtonElement.labelDescription = 'bar';

        // Assert.
        const buttonLabel = hoverButtonElement.shadowRoot!.querySelector(
            'customize-chrome-button-label');
        assertEquals(hoverButtonElement.label, buttonLabel!.label);
        assertEquals(
            hoverButtonElement.labelDescription, buttonLabel!.labelDescription);
      });

  test('Enter key triggers click event', async () => {
    const clickEventHappened: boolean = await new Promise(resolve => {
      listenOnce(hoverButtonElement, 'click', () => {
        resolve(true);
      });
      keyDownOn(hoverButtonElement, 0, [], 'Enter');
    });
    assertTrue(clickEventHappened);
  });

  test('Space key triggers click event', async () => {
    const clickEventHappened: boolean = await new Promise(resolve => {
      listenOnce(hoverButtonElement, 'click', () => {
        resolve(true);
      });
      keyDownOn(hoverButtonElement, 0, [], ' ');
    });
    assertTrue(clickEventHappened);
  });
});
