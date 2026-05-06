// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/composebox/composebox_input.js';

import type {ComposeboxInputElement} from 'chrome://resources/cr_components/composebox/composebox_input.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

async function pollUntil(predicate: () => boolean, timeoutMs = 1000):
    Promise<void> {
  const start = Date.now();
  while (!predicate()) {
    if (Date.now() - start > timeoutMs) {
      throw new Error('pollUntil timed out');
    }
    await new Promise<void>(resolve => requestAnimationFrame(() => resolve()));
  }
}

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

  test('smartComposeEnabled reflects to host attribute', async () => {
    inputElement.smartComposeEnabled = false;
    await inputElement.updateComplete;
    assertFalse(inputElement.hasAttribute('smart-compose-enabled'));

    inputElement.smartComposeEnabled = true;
    await inputElement.updateComplete;
    assertTrue(inputElement.hasAttribute('smart-compose-enabled'));
  });

  test('#smartCompose is hidden when smartComposeEnabled is false',
    async () => {
      inputElement.smartComposeInlineHint = 'foo';
      inputElement.smartComposeEnabled = false;
      await inputElement.updateComplete;

      const smartCompose =
          inputElement.shadowRoot.querySelector<HTMLElement>('#smartCompose');
      assertTrue(!!smartCompose);
      assertEquals('none', getComputedStyle(smartCompose).display);
  });

  test('#smartCompose is visible when smartComposeEnabled is true',
    async () => {
      inputElement.smartComposeInlineHint = 'foo';
      inputElement.smartComposeEnabled = true;
      await inputElement.updateComplete;

      const smartCompose =
          inputElement.shadowRoot.querySelector<HTMLElement>('#smartCompose');
      assertTrue(!!smartCompose);
      assertTrue(getComputedStyle(smartCompose).display !== 'none');
  });

  test('#tabChip has fixed 38*24 box model', async () => {
    inputElement.smartComposeInlineHint = 'foo';
    inputElement.smartComposeEnabled = true;
    await inputElement.updateComplete;

    const chip =
        inputElement.shadowRoot.querySelector<HTMLElement>('#tabChip');
    assertTrue(!!chip);

    const rect = chip.getBoundingClientRect();
    assertEquals(38, rect.width);
    assertEquals(24, rect.height);

    const computed = getComputedStyle(chip);
    assertEquals('0px', computed.paddingTop);
    assertEquals('0px', computed.paddingBottom);
    assertEquals('0px', computed.paddingLeft);
    assertEquals('0px', computed.paddingRight);
    assertEquals('inline-flex', computed.display);
    assertEquals('18px', computed.lineHeight);
  });

  test('input minHeight stays unset for short hint', async () => {
    inputElement.smartComposeEnabled = true;
    inputElement.smartComposeInlineHint = 'short';
    await inputElement.updateComplete;
    await new Promise<void>(
        resolve => requestAnimationFrame(
            () => requestAnimationFrame(() => resolve())));

    const input = inputElement.$.input;
    const smartCompose =
        inputElement.shadowRoot.querySelector<HTMLElement>('#smartCompose');
    assertTrue(!!smartCompose);
    assertEquals('', input.style.minHeight);
    assertEquals('', smartCompose.style.minHeight);
  });

  test('input minHeight extends for multi-line hint', async () => {
    inputElement.smartComposeEnabled = true;
    inputElement.smartComposeInlineHint = 'first line\nsecond line';
    await inputElement.updateComplete;

    const input = inputElement.$.input;
    await pollUntil(() => input.style.minHeight !== '');

    const smartCompose =
        inputElement.shadowRoot.querySelector<HTMLElement>('#smartCompose');
    assertTrue(!!smartCompose);
    assertTrue(input.style.minHeight !== '');
    assertEquals('', smartCompose.style.minHeight);
  });

  test('inline minHeight is reset when hint clears', async () => {
    inputElement.smartComposeEnabled = true;
    inputElement.smartComposeInlineHint = 'first line\nsecond line';
    await inputElement.updateComplete;

    const input = inputElement.$.input;
    await pollUntil(() => input.style.minHeight !== '');
    assertTrue(input.style.minHeight !== '');

    inputElement.smartComposeInlineHint = '';
    await inputElement.updateComplete;
    await pollUntil(() => input.style.minHeight === '');

    assertEquals('', input.style.minHeight);
  });

  test(
      'minHeight tracks input growth even when hint is unchanged', async () => {
        inputElement.smartComposeEnabled = true;
        inputElement.smartComposeInlineHint = 'hint a\nhint b';
        await inputElement.updateComplete;

        const input = inputElement.$.input;
        await pollUntil(() => input.style.minHeight !== '');

        const initialMinHeight = input.style.minHeight;
        assertTrue(initialMinHeight !== '');

        inputElement.input = '\n';
        await inputElement.updateComplete;
        await pollUntil(
          () => parseInt(input.style.minHeight, 10) >
                    parseInt(initialMinHeight, 10));

        const newMinHeight = input.style.minHeight;
        assertTrue(newMinHeight !== '');
        assertTrue(parseInt(newMinHeight, 10) > parseInt(initialMinHeight, 10));
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

  test('CaretAnchorStableDuringScroll', async () => {
    const input = inputElement.$.input as HTMLTextAreaElement;
    const mirror =
        inputElement.shadowRoot.querySelector<HTMLElement>('#mirror');
    const inputWrapper =
        inputElement.shadowRoot.querySelector<HTMLElement>('#inputWrapper');
    assertTrue(!!input);
    assertTrue(!!mirror);
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

    // The last mirror span should be the anchor.
    const lastSpan = mirror.childNodes[longText.length - 1] as HTMLElement;
    assertTrue(!!lastSpan);
    assertEquals('--cursor-char', lastSpan.style.anchorName);

    // Scroll the wrapper to the top.
    inputWrapper.scrollTop = 0;
    await microtasksFinished();

    // The anchor span should remain the same after scrolling.
    assertEquals('--cursor-char', lastSpan.style.anchorName);
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
  test('CaretAnchorUpdatesOnInputWrapperWidthChange', async () => {
    const input = inputElement.$.input as HTMLTextAreaElement;
    const mirror =
        inputElement.shadowRoot.querySelector<HTMLElement>('#mirror');
    const inputWrapper =
        inputElement.shadowRoot.querySelector<HTMLElement>('#inputWrapper');
    assertTrue(!!input);
    assertTrue(!!mirror);
    assertTrue(!!inputWrapper);

    input.value = 'Hello world';
    input.dispatchEvent(new Event('input', {bubbles: true}));
    input.setSelectionRange(11, 11);
    input.dispatchEvent(new Event('keyup', {bubbles: true}));
    await inputElement.updateComplete;

    // The anchor should be on the last span (char before cursor).
    const anchoredSpan = mirror.childNodes[10] as HTMLElement;
    assertTrue(!!anchoredSpan);
    assertEquals('--cursor-char', anchoredSpan.style.anchorName);

    inputWrapper.style.width = '20px';

    await new Promise(resolve => setTimeout(resolve, 50));
    await new Promise(resolve => requestAnimationFrame(resolve));
    await microtasksFinished();

    // After width change triggers re-layout, the anchor should still be set on
    // a mirror span (the updateCaret_ re-runs via ResizeObserver).
    const spans = mirror.querySelectorAll('span');
    const anchoredSpans =
        Array.from(spans).filter(s => s.style.anchorName === '--cursor-char');
    assertEquals(1, anchoredSpans.length);
  });

  test('CaretAnchorDoesNotUpdateOnHeightOnlyChange', async () => {
    const input = inputElement.$.input as HTMLTextAreaElement;
    const mirror =
        inputElement.shadowRoot.querySelector<HTMLElement>('#mirror');
    const inputWrapper =
        inputElement.shadowRoot.querySelector<HTMLElement>('#inputWrapper');
    assertTrue(!!input);
    assertTrue(!!mirror);
    assertTrue(!!inputWrapper);

    input.value = 'Hey world';
    input.dispatchEvent(new Event('input', {bubbles: true}));
    input.setSelectionRange(9, 9);
    input.dispatchEvent(new Event('keyup', {bubbles: true}));
    await inputElement.updateComplete;

    // The anchor should be on the span before the cursor (index 8).
    const anchoredSpan = mirror.childNodes[8] as HTMLElement;
    assertTrue(!!anchoredSpan);
    assertEquals('--cursor-char', anchoredSpan.style.anchorName);
    const widthBefore = inputWrapper.clientWidth;

    inputWrapper.style.paddingBottom = '10px';

    await new Promise(resolve => setTimeout(resolve, 50));
    await new Promise(resolve => requestAnimationFrame(resolve));
    await microtasksFinished();

    assertEquals(widthBefore, inputWrapper.clientWidth);

    // The same span should still be the anchor (no spurious update).
    assertEquals('--cursor-char', anchoredSpan.style.anchorName);
  });
});

suite('ComposeboxCaretGeometry', () => {
  let inputElement: ComposeboxInputElement;
  let originalDir: string;

  setup(async () => {
    originalDir = document.documentElement.dir;
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    inputElement = document.createElement('cr-composebox-input');
    document.body.appendChild(inputElement);
    await inputElement.updateComplete;
  });

  teardown(() => {
    document.documentElement.dir = originalDir;
  });

  // Verify the caret's rendered position aligns with its anchor span.
  test('CaretRenderedPositionMatchesAnchorSpanLtr', async () => {
    document.documentElement.dir = 'ltr';
    const input = inputElement.$.input as HTMLTextAreaElement;
    const caret = inputElement.shadowRoot.querySelector<HTMLElement>('#caret');
    const mirror =
        inputElement.shadowRoot.querySelector<HTMLElement>('#mirror');
    assertTrue(!!caret);
    assertTrue(!!mirror);

    input.value = 'Hello';
    input.dispatchEvent(new Event('input', {bubbles: true}));
    input.setSelectionRange(5, 5);
    input.dispatchEvent(new Event('keyup', {bubbles: true}));
    await inputElement.updateComplete;

    // Force focus to make the caret visible (display: block)
    input.focus();
    await inputElement.updateComplete;

    const anchoredSpan = mirror.childNodes[4] as HTMLElement;
    assertTrue(!!anchoredSpan);

    const caretRect = caret.getBoundingClientRect();
    const spanRect = anchoredSpan.getBoundingClientRect();

    // In LTR, the caret's left edge should be at the span's right edge.
    assertTrue(Math.abs(caretRect.left - spanRect.right) < 2);

    // The caret's top should be near the span's top (within the 2px offset)
    assertTrue(Math.abs(caretRect.top - (spanRect.top - 2)) < 2);
  });

  test('CaretRenderedPositionMatchesAnchorSpanRtl', async () => {
    document.documentElement.dir = 'rtl';
    const input = inputElement.$.input as HTMLTextAreaElement;
    const caret = inputElement.shadowRoot.querySelector<HTMLElement>('#caret');
    const mirror =
        inputElement.shadowRoot.querySelector<HTMLElement>('#mirror');
    assertTrue(!!caret);
    assertTrue(!!mirror);

    input.value = 'Hello';
    input.dispatchEvent(new Event('input', {bubbles: true}));
    input.setSelectionRange(5, 5);
    input.dispatchEvent(new Event('keyup', {bubbles: true}));
    await inputElement.updateComplete;

    input.focus();
    await inputElement.updateComplete;

    const anchoredSpan = mirror.childNodes[4] as HTMLElement;
    assertTrue(!!anchoredSpan);

    const caretRect = caret.getBoundingClientRect();
    const spanRect = anchoredSpan.getBoundingClientRect();

    // In RTL, `left: anchor(end)` resolves to the span's left (start) edge.
    assertTrue(Math.abs(caretRect.left - spanRect.left) < 2);

    // The caret's top should be near the span's top (within the 2px offset)
    assertTrue(Math.abs(caretRect.top - (spanRect.top - 2)) < 2);
  });

  test('CaretAtStartPositionedAtFirstSpanStart', async () => {
    const input = inputElement.$.input as HTMLTextAreaElement;
    const caret = inputElement.shadowRoot.querySelector<HTMLElement>('#caret');
    const mirror =
        inputElement.shadowRoot.querySelector<HTMLElement>('#mirror');
    assertTrue(!!caret);
    assertTrue(!!mirror);

    input.value = 'AB';
    input.dispatchEvent(new Event('input', {bubbles: true}));
    input.setSelectionRange(0, 0);
    input.dispatchEvent(new Event('keyup', {bubbles: true}));
    await inputElement.updateComplete;

    input.focus();
    await inputElement.updateComplete;

    const firstSpan = mirror.firstChild as HTMLElement;
    assertTrue(!!firstSpan);
    assertEquals('--cursor-char', firstSpan.style.anchorName);
    assertTrue(caret.classList.contains('at-start'));

    const spanRect = firstSpan.getBoundingClientRect();
    const caretRect = caret.getBoundingClientRect();

    // At position 0 with `at-start`, caret's left should be at span's left
    // (start), not span's right (end).
    assertTrue(Math.abs(caretRect.left - spanRect.left) < 2);
  });

  test('ResetCaretAnchorsToFirstSpan', async () => {
    const input = inputElement.$.input as HTMLTextAreaElement;
    const caret = inputElement.shadowRoot.querySelector<HTMLElement>('#caret');
    const mirror =
        inputElement.shadowRoot.querySelector<HTMLElement>('#mirror');
    assertTrue(!!caret);
    assertTrue(!!mirror);

    // Type text and move cursor to the middle.
    input.value = 'Hello world';
    input.dispatchEvent(new Event('input', {bubbles: true}));
    input.setSelectionRange(5, 5);
    input.dispatchEvent(new Event('keyup', {bubbles: true}));
    await inputElement.updateComplete;

    // Cursor should be anchored at index 4 (char before cursor).
    const midSpan = mirror.childNodes[4] as HTMLElement;
    assertTrue(!!midSpan);
    assertEquals('--cursor-char', midSpan.style.anchorName);

    // Call resetCaret_ should anchor to first span, not current selection.
    inputElement.resetCaret();

    const firstSpan = mirror.firstChild as HTMLElement;
    assertTrue(!!firstSpan);
    assertEquals('--cursor-char', firstSpan.style.anchorName);
    assertTrue(caret.classList.contains('at-start'));

    // The mid span should no longer be the anchor.
    assertEquals('', midSpan.style.anchorName);
  });
});
