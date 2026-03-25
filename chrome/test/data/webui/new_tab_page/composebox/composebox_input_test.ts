// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/composebox/composebox_input.js';

import type {ComposeboxInputElement} from 'chrome://resources/cr_components/composebox/composebox_input.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('ComposeboxInputTest', () => {
  let inputElement: ComposeboxInputElement;

  setup(async () => {
    loadTimeData.resetForTesting({
      composeboxSmartComposeTabTitle: 'Tab',
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    inputElement = document.createElement('cr-composebox-input');
    document.body.appendChild(inputElement);
    await inputElement.updateComplete;
  });

  test('Input updates mirror on type', async () => {
    inputElement.input = 'hello world';
    await inputElement.updateComplete;

    const mirror =
        inputElement.shadowRoot.querySelector<HTMLElement>('#mirror');
    assertTrue(!!mirror);
    // The mirror has spans for each character
    assertEquals(11, mirror.querySelectorAll('span').length);
  });

  test('Cancel button click fires event', () => {
    let cancelClicked = false;
    inputElement.addEventListener('cancel-click', () => {
      cancelClicked = true;
    });

    const cancelIcon =
        inputElement.shadowRoot.querySelector<HTMLElement>('#cancelIcon');
    assertTrue(!!cancelIcon);
    cancelIcon.click();

    assertTrue(cancelClicked);
  });

  test(
      'Cancel button visibility based on isCollapsible and submitEnabled',
      async () => {
        const cancelIcon =
            inputElement.shadowRoot.querySelector<HTMLElement>('#cancelIcon');
        assertTrue(!!cancelIcon);

        inputElement.isCollapsible = true;
        inputElement.submitEnabled = false;
        await inputElement.updateComplete;

        assertEquals('0', window.getComputedStyle(cancelIcon).opacity);
        assertEquals('none', window.getComputedStyle(cancelIcon).pointerEvents);

        inputElement.submitEnabled = true;
        await inputElement.updateComplete;

        assertEquals('1', window.getComputedStyle(cancelIcon).opacity);
        assertEquals('auto', window.getComputedStyle(cancelIcon).pointerEvents);
      });

  test('Input scroll syncs with smart compose', async () => {
    inputElement.smartComposeInlineHint = 'some hint';
    await inputElement.updateComplete;

    const smartCompose =
        inputElement.shadowRoot.querySelector<HTMLElement>('#smartCompose');
    assertTrue(!!smartCompose);

    const textArea = inputElement.$.input;

    // Mock the scrollTop properties since the elements might not be scrollable
    // in the test environment.
    let smartComposeScrollTop = 0;
    Object.defineProperty(smartCompose, 'scrollTop', {
      get: () => smartComposeScrollTop,
      set: (v) => {
        smartComposeScrollTop = v;
      },
    });

    Object.defineProperty(textArea, 'scrollTop', {
      get: () => 50,
      set: () => {},
    });

    textArea.dispatchEvent(new Event('scroll'));

    assertEquals(50, smartCompose.scrollTop);
  });

  test('Events are forwarded from input', () => {
    const textArea = inputElement.$.input;

    let inputFired = false;
    inputElement.addEventListener('input-input', () => {
      inputFired = true;
    });

    textArea.value = 'test';
    textArea.dispatchEvent(new Event('input'));
    assertTrue(inputFired);
    assertEquals('test', inputElement.input);

    let keyupFired = false;
    inputElement.addEventListener('input-keyup', () => {
      keyupFired = true;
    });
    textArea.dispatchEvent(new KeyboardEvent('keyup', {key: 'Enter'}));
    assertTrue(keyupFired);

    let clickFired = false;
    inputElement.addEventListener('input-click', () => {
      clickFired = true;
    });
    textArea.dispatchEvent(new MouseEvent('click'));
    assertTrue(clickFired);
  });
});
