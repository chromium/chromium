// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/hover_button.js';

import type {HoverButtonElement} from 'chrome://customize-chrome-side-panel.top-chrome/hover_button.js';
import {listenOnce} from 'chrome://resources/js/util.js';
import {assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('HoverButtonTest', () => {
  let hoverButtonElement: HoverButtonElement;

  setup(() => {
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
        await microtasksFinished();

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
        await microtasksFinished();

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

  test('icon shows', () => {
    // Set --cr-icon-image variable. In prod code this is done in a parent
    // element.
    const crIconImage = 'url("chrome://resources/images/open_in_new.svg")';
    hoverButtonElement.style.setProperty('--cr-icon-image', crIconImage);

    // Assert that icon is visible.
    const icon = hoverButtonElement.shadowRoot!.querySelector<HTMLElement>(
        '#icon.cr-icon');
    assertTrue(!!icon);
    assertTrue(isVisible(icon));

    // Assert that `--cr-icon-image` propagates to the icon's `mask-image`
    // property.
    const maskImageProperty = icon.computedStyleMap().get('mask-image');
    assertTrue(!!maskImageProperty);
    assertEquals(crIconImage, maskImageProperty.toString());
  });

  test('focus transfers to inner button', () => {
    assertNotEquals(
        hoverButtonElement.shadowRoot!.activeElement,
        hoverButtonElement.$.hoverButton);

    hoverButtonElement.focus();

    assertEquals(
        hoverButtonElement.shadowRoot!.activeElement,
        hoverButtonElement.$.hoverButton);
  });
});
