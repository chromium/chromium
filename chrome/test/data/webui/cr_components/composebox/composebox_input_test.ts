// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/composebox/composebox_input.js';

import type {ComposeboxInputElement} from 'chrome://resources/cr_components/composebox/composebox_input.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

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

suite('ComposeboxScrollCaret', () => {
  let inputElement: ComposeboxInputElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    inputElement = document.createElement('cr-composebox-input');
    inputElement.style.setProperty('--text-input-max-height', '100px');
    inputElement.style.setProperty('--text-input-top-spacing', '8px');
    document.body.appendChild(inputElement);
    await inputElement.updateComplete;
  });

  test('InputWrapperIsScrollContainer', () => {
    const inputWrapper =
        inputElement.shadowRoot.querySelector<HTMLElement>('#inputWrapper');
    assertTrue(!!inputWrapper);

    const overflowY = window.getComputedStyle(inputWrapper).overflowY;
    assertEquals('auto', overflowY);
  });

  test('TextareaDoesNotScrollInternally', () => {
    const input = inputElement.$.input;
    assertTrue(!!input);

    const maxHeight = window.getComputedStyle(input).maxHeight;
    assertEquals('none', maxHeight);
  });

  test('CaretTransformStableDuringScroll', async () => {
    const input = inputElement.$.input as HTMLTextAreaElement;
    const caret = inputElement.shadowRoot.querySelector<HTMLElement>('#caret');
    const inputWrapper =
        inputElement.shadowRoot.querySelector<HTMLElement>('#inputWrapper');
    assertTrue(!!input);
    assertTrue(!!caret);
    assertTrue(!!inputWrapper);

    // Type enough texts to cause scrolling.
    const longText = Array(100).fill('Let\'s keep typing longer...').join('\n');
    input.value = longText;
    input.dispatchEvent(new Event('input', {bubbles: true}));
    await inputElement.updateComplete;

    // Place caret at the end.
    input.setSelectionRange(longText.length, longText.length);
    input.dispatchEvent(new Event('keyup', {bubbles: true}));
    await inputElement.updateComplete;

    // Verify that the wrapper has scrollable content.
    assertTrue(inputWrapper.scrollHeight > inputWrapper.clientHeight);

    // Record the caret transform before scrolling.
    const caretTransformBeforeScroll = caret.style.transform;
    assertTrue(caretTransformBeforeScroll.length > 0);

    // Scroll the wrapper to the top.
    inputWrapper.scrollTop = 0;
    await microtasksFinished();

    // Verify that the caret transform is the same before and after scrolling.
    assertEquals(caretTransformBeforeScroll, caret.style.transform);
  });

  test('MaskImageOnWrapper', () => {
    const inputWrapper =
        inputElement.shadowRoot.querySelector<HTMLElement>('#inputWrapper');
    assertTrue(!!inputWrapper);

    // The mask-image should be on the input wrapper.
    const wrapperMask =
        window.getComputedStyle(inputWrapper).getPropertyValue('mask-image');
    assertTrue(wrapperMask.length > 0 && wrapperMask !== 'none');
  });

  test('TextareaUsesFieldSizingContent', () => {
    const input = inputElement.$.input;
    assertTrue(!!input);

    const fieldSizing =
        window.getComputedStyle(input).getPropertyValue('field-sizing');
    assertEquals('content', fieldSizing);
  });

  // The caret resize observer should only react to width changes on
  // #inputWrapper, not height-only changes that can feed back into a layout loop
  // e.g. Windows non-overlay scrollbar toggling.
  test('CaretUpdatesOnInputWrapperWidthChange', async () => {
    const input = inputElement.$.input as HTMLTextAreaElement;
    const caret = inputElement.shadowRoot.querySelector<HTMLElement>('#caret');
    const inputWrapper =
        inputElement.shadowRoot.querySelector<HTMLElement>('#inputWrapper');
    assertTrue(!!input);
    assertTrue(!!caret);
    assertTrue(!!inputWrapper);

    input.value = 'Hello world';
    input.dispatchEvent(new Event('input', {bubbles: true}));
    input.setSelectionRange(11, 11);
    input.dispatchEvent(new Event('keyup', {bubbles: true}));
    await inputElement.updateComplete;

    const caretTransformBefore = caret.style.transform;
    assertTrue(caretTransformBefore.length > 0);

    inputWrapper.style.width = '20px';

    await new Promise(resolve => setTimeout(resolve, 50));
    await new Promise(resolve => requestAnimationFrame(resolve));
    await microtasksFinished();

    assertTrue(caretTransformBefore !== caret.style.transform);
  });

  test('CaretDoesNotUpdateOnHeightOnlyChange', async () => {
    const input = inputElement.$.input as HTMLTextAreaElement;
    const caret = inputElement.shadowRoot.querySelector<HTMLElement>('#caret');
    const inputWrapper =
        inputElement.shadowRoot.querySelector<HTMLElement>('#inputWrapper');
    assertTrue(!!input);
    assertTrue(!!caret);
    assertTrue(!!inputWrapper);

    input.value = 'Hey world';
    input.dispatchEvent(new Event('input', {bubbles: true}));
    input.setSelectionRange(10, 10);
    input.dispatchEvent(new Event('keyup', {bubbles: true}));
    await inputElement.updateComplete;

    const caretTransformBefore = caret.style.transform;
    const widthBefore = inputWrapper.clientWidth;

    inputWrapper.style.paddingBottom = '10px';

    await new Promise(resolve => setTimeout(resolve, 50));
    await new Promise(resolve => requestAnimationFrame(resolve));
    await microtasksFinished();

    assertEquals(widthBefore, inputWrapper.clientWidth);

    assertEquals(caretTransformBefore, caret.style.transform);
  });
});
