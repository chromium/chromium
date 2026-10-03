// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/composebox/composebox_input.js';

import {CHIP_CLASS, CHIP_LABEL_CLASS, createChipElement, getChipPolicyForTesting, NON_BREAKING_SPACE} from 'chrome://resources/cr_components/composebox/composebox_input.js';
import type {ComposeboxInputElement} from 'chrome://resources/cr_components/composebox/composebox_input.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

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
    const textArea =
        inputElement.shadowRoot.querySelector<HTMLTextAreaElement>('#input')!;

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

  test('input minHeight extends for multi-line hint', async () => {
    inputElement.smartComposeEnabled = true;
    inputElement.smartComposeInlineHint = 'first line\nsecond line';
    await inputElement.updateComplete;

    const input = inputElement.shadowRoot.querySelector<HTMLElement>('#input')!;
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

    const input = inputElement.shadowRoot.querySelector<HTMLElement>('#input')!;
    await pollUntil(() => input.style.minHeight !== '');
    assertTrue(input.style.minHeight !== '');

    inputElement.smartComposeInlineHint = '';
    await inputElement.updateComplete;
    await pollUntil(() => input.style.minHeight === '');

    assertEquals('', input.style.minHeight);
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
    const input = inputElement.shadowRoot.querySelector<HTMLElement>('#input')!;
    assertTrue(!!input);

    const maxHeight = window.getComputedStyle(input).maxHeight;
    assertEquals('none', maxHeight);
  });

  test('CaretAnchorStableDuringScroll', async () => {
    const input =
        inputElement.shadowRoot.querySelector<HTMLTextAreaElement>('#input')!;
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
    const input = inputElement.shadowRoot.querySelector<HTMLElement>('#input')!;
    assertTrue(!!input);

    const fieldSizing =
        window.getComputedStyle(input).getPropertyValue('field-sizing');
    assertEquals('content', fieldSizing);
  });

  // The caret resize observer should only react to width changes on
  // #inputWrapper, not height-only changes that can feed back into a layout loop
  // e.g. Windows non-overlay scrollbar toggling.
  test('CaretAnchorUpdatesOnInputWrapperWidthChange', async () => {
    const input =
        inputElement.shadowRoot.querySelector<HTMLTextAreaElement>('#input')!;
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
    const input =
        inputElement.shadowRoot.querySelector<HTMLTextAreaElement>('#input')!;
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

  test(
      'inputWrapper minHeight does not shrink when text is partially deleted',
      async () => {
        document.body.style.width = '800px';
        document.body.style.height = '600px';

        const wrapper = inputElement.shadowRoot.getElementById('inputWrapper')!;
        const input =
            inputElement.shadowRoot.querySelector<HTMLTextAreaElement>(
                '#input')!;

        // Initially minHeight is empty.
        assertEquals('', wrapper.style.minHeight);

        // Type some text to make the textarea grow.
        input.value = 'line 1\nline 2\nline 3\nline 4\nline 5';
        input.dispatchEvent(new Event('input', {bubbles: true}));
        await inputElement.updateComplete;

        // Wait for the ResizeObserver to run and lock the height.
        await pollUntil(() => wrapper.style.minHeight !== '');

        const initialMinHeight = wrapper.style.minHeight;
        assertTrue(initialMinHeight !== '');

        // Now partially delete the text (not clearing all text).
        input.value = 'line 1\nline 2';
        input.dispatchEvent(new Event('input', {bubbles: true}));
        await inputElement.updateComplete;

        // Wait a bit to ensure ResizeObserver has chance to run.
        await new Promise<void>(
            resolve => requestAnimationFrame(
                () => requestAnimationFrame(() => resolve())));

        // Verify minHeight is still locked to the larger height.
        assertEquals(initialMinHeight, wrapper.style.minHeight);
      });

  test('inputWrapper minHeight resets when all text is cleared', async () => {
    document.body.style.width = '800px';
    document.body.style.height = '600px';

    const wrapper = inputElement.shadowRoot.getElementById('inputWrapper')!;
    const input =
        inputElement.shadowRoot.querySelector<HTMLTextAreaElement>('#input')!;

    // Type some text to make the textarea grow.
    input.value = 'line 1\nline 2\nline 3\nline 4\nline 5';
    input.dispatchEvent(new Event('input', {bubbles: true}));
    await inputElement.updateComplete;
    await pollUntil(() => wrapper.style.minHeight !== '');

    assertTrue(wrapper.style.minHeight !== '');

    // Now clear all the text.
    input.value = '';
    input.dispatchEvent(new Event('input', {bubbles: true}));
    await inputElement.updateComplete;

    // Verify minHeight resets as soon as text is cleared.
    assertEquals('', wrapper.style.minHeight);
    assertEquals('', input.style.minHeight);
  });

  test('resetHeight clears minHeight from input and wrapper', async () => {
    // Style body/host to allow layout
    document.body.style.width = '800px';
    document.body.style.height = '600px';
    inputElement.style.width = '100%';

    const wrapper =
        inputElement.shadowRoot.querySelector<HTMLElement>('#inputWrapper');
    const input =
        inputElement.shadowRoot.querySelector<HTMLTextAreaElement>('#input')!;
    assertTrue(!!wrapper);

    // 1. Lock the wrapper's height by entering multiline text.
    input.value = 'line 1\nline 2\nline 3\nline 4\nline 5';
    input.dispatchEvent(new Event('input', {bubbles: true}));
    await inputElement.updateComplete;
    await pollUntil(() => wrapper.style.minHeight !== '');

    // 2. Set some minHeight on the input (simulate smart compose).
    input.style.minHeight = '120px';

    // Verify they are not empty.
    assertTrue(wrapper.style.minHeight !== '');
    assertEquals('120px', input.style.minHeight);

    // 3. Reset height.
    inputElement.resetHeight();

    // Verify they are reset.
    assertEquals('', wrapper.style.minHeight);
    assertEquals('', input.style.minHeight);
  });

  test(
      'ResizeObserver and rAF: stale callback does not write old height and new lock establishes for short text',
      async () => {
        document.body.style.width = '800px';
        document.body.style.height = '600px';
        inputElement.style.width = '100%';
        inputElement.style.setProperty('--text-input-max-height', '500px');

        const wrapper =
            inputElement.shadowRoot.querySelector<HTMLElement>('#inputWrapper');
        const input =
            inputElement.shadowRoot.querySelector<HTMLTextAreaElement>(
                '#input')!;
        assertTrue(!!wrapper);

        const capturedCallbacks: FrameRequestCallback[] = [];
        let captureRaf = false;
        const originalRaf = window.requestAnimationFrame;

        try {
          window.requestAnimationFrame = (cb: FrameRequestCallback) => {
            if (captureRaf) {
              capturedCallbacks.push(cb);
              return capturedCallbacks.length;
            }
            return originalRaf(cb);
          };

          // 1. Enter tall multiline text and capture pending rAF callbacks.
          captureRaf = true;
          input.value = 'line 1\nline 2\nline 3\nline 4\nline 5\nline 6';
          input.dispatchEvent(new Event('input', {bubbles: true}));
          await inputElement.updateComplete;

          await new Promise<void>(
              resolve => originalRaf(() => originalRaf(() => resolve())));
          assertTrue(capturedCallbacks.length > 0);
          captureRaf = false;

          const tallHeight = wrapper.clientHeight;
          assertTrue(tallHeight > 50);

          // 2. Reset height to increment generation and clear minHeight.
          inputElement.resetHeight();
          assertEquals('', wrapper.style.minHeight);

          // 3. Enter shorter text and allow its new rAF to execute naturally.
          input.value = 'line 1\nline 2';
          input.dispatchEvent(new Event('input', {bubbles: true}));
          await inputElement.updateComplete;

          await pollUntil(() => wrapper.style.minHeight !== '');
          const shortLockHeight = wrapper.style.minHeight;
          assertTrue(parseFloat(shortLockHeight) < tallHeight);

          // 4. Execute the stale rAF callbacks from the tall text.
          for (const cb of capturedCallbacks) {
            cb(performance.now());
          }

          // Stale callback must be ignored due to generation mismatch;
          // the short lock must not be overwritten by the tall height.
          assertEquals(shortLockHeight, wrapper.style.minHeight);
          assertTrue(parseFloat(wrapper.style.minHeight) < tallHeight);

          // 5. Verify the new height lock is active and prevents shrinking on
          // delete.
          input.value = 'line 1';
          input.dispatchEvent(new Event('input', {bubbles: true}));
          await inputElement.updateComplete;
          await new Promise<void>(
              resolve => originalRaf(() => originalRaf(() => resolve())));
          assertEquals(shortLockHeight, wrapper.style.minHeight);
        } finally {
          window.requestAnimationFrame = originalRaf;
        }
      });
});

suite('ComposeboxCaretGeometry', () => {
  let inputElement: ComposeboxInputElement;
  let originalDir: string;

  function setDirection(dir: 'ltr'|'rtl') {
    document.documentElement.dir = dir;
    inputElement.updateDirection();
  }

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
    setDirection('ltr');
    const input =
        inputElement.shadowRoot.querySelector<HTMLTextAreaElement>('#input')!;
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
    input.dispatchEvent(new FocusEvent('focus'));
    await inputElement.updateComplete;
    await microtasksFinished();

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
    setDirection('rtl');
    const input =
        inputElement.shadowRoot.querySelector<HTMLTextAreaElement>('#input')!;
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
    input.dispatchEvent(new FocusEvent('focus'));
    await inputElement.updateComplete;
    await microtasksFinished();

    const anchoredSpan = mirror.firstChild as HTMLElement;
    assertTrue(!!anchoredSpan);

    const caretRect = caret.getBoundingClientRect();
    const spanRect = anchoredSpan.getBoundingClientRect();

    // In RTL, the caret should be at the left edge of the leftmost span.
    assertTrue(Math.abs(caretRect.left - spanRect.left) < 2);

    // The caret's top should be near the span's top (within the 2px offset)
    assertTrue(Math.abs(caretRect.top - (spanRect.top - 2)) < 2);
  });

  test('CaretPositionedAtFarLeftOfLtrTextInRtl', async () => {
    setDirection('rtl');
    const input =
        inputElement.shadowRoot.querySelector<HTMLTextAreaElement>('#input')!;
    const caret = inputElement.shadowRoot.querySelector<HTMLElement>('#caret');
    const mirror =
        inputElement.shadowRoot.querySelector<HTMLElement>('#mirror');
    assertTrue(!!caret);
    assertTrue(!!mirror);

    input.value = 'AB';
    input.dispatchEvent(new Event('input', {bubbles: true}));
    input.setSelectionRange(2, 2);
    input.dispatchEvent(new Event('keyup', {bubbles: true}));
    await inputElement.updateComplete;

    input.focus();
    input.dispatchEvent(new FocusEvent('focus'));
    await inputElement.updateComplete;
    await microtasksFinished();

    const firstSpan = mirror.childNodes[0] as HTMLElement;
    const secondSpan = mirror.childNodes[1] as HTMLElement;
    assertTrue(!!firstSpan);
    assertTrue(!!secondSpan);

    const caretRect = caret.getBoundingClientRect();
    const firstSpanRect = firstSpan.getBoundingClientRect();
    const secondSpanRect = secondSpan.getBoundingClientRect();

    // Caret should be at the left edge of the first span ('A').
    assertTrue(Math.abs(caretRect.left - firstSpanRect.left) < 2);

    // Caret should NOT be at the left edge of the second span ('B').
    assertTrue(Math.abs(caretRect.left - secondSpanRect.left) > 5);
  });

  test('CaretAtStartPositionedAtFirstSpanStart', async () => {
    const input =
        inputElement.shadowRoot.querySelector<HTMLTextAreaElement>('#input')!;
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
    input.dispatchEvent(new FocusEvent('focus'));
    await inputElement.updateComplete;
    await microtasksFinished();

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
    const input =
        inputElement.shadowRoot.querySelector<HTMLTextAreaElement>('#input')!;
    const caret = inputElement.shadowRoot.querySelector<HTMLElement>('#caret');
    const mirror =
        inputElement.shadowRoot.querySelector<HTMLElement>('#mirror');
    assertTrue(!!caret);
    assertTrue(!!mirror);

    // Type text and move cursor to the middle.
    input.focus();
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

  test('CaretPlacedAtEndWhenSkillsEnabledAndInputChanges', async () => {
    inputElement.composeboxSkillsEnabled = true;
    await inputElement.updateComplete;

    const input = inputElement.shadowRoot.querySelector<HTMLElement>('#input')!;
    const caret = inputElement.shadowRoot.querySelector<HTMLElement>('#caret');
    const mirror =
        inputElement.shadowRoot.querySelector<HTMLElement>('#mirror');
    assertTrue(!!input);
    assertTrue(!!caret);
    assertTrue(!!mirror);

    input.focus();
    await inputElement.updateComplete;

    inputElement.input = 'hello world';
    await inputElement.updateComplete;
    await microtasksFinished();

    assertEquals(11, inputElement.getSelectionEnd());

    const lastSpan = mirror.childNodes[10] as HTMLElement;
    assertTrue(!!lastSpan);
    assertEquals('--cursor-char', lastSpan.style.anchorName);
    assertFalse(caret.classList.contains('at-start'));
  });

  test('CaretWrapsToNextLineOnConsecutiveSpaces', async () => {
    inputElement.style.width = '100px';
    const input =
        inputElement.shadowRoot.querySelector<HTMLTextAreaElement>('#input')!;
    const caret = inputElement.shadowRoot.querySelector<HTMLElement>('#caret');
    const mirror =
        inputElement.shadowRoot.querySelector<HTMLElement>('#mirror');
    assertTrue(!!caret);
    assertTrue(!!mirror);

    // Type spaces that exceed the container width.
    const spaces = ' '.repeat(50);
    input.value = spaces;
    input.dispatchEvent(new Event('input', {bubbles: true}));
    input.setSelectionRange(spaces.length, spaces.length);
    input.dispatchEvent(new Event('keyup', {bubbles: true}));
    await inputElement.updateComplete;

    input.focus();
    await inputElement.updateComplete;
    await microtasksFinished();

    const firstSpan = mirror.childNodes[0] as HTMLElement;
    const lastSpan = mirror.childNodes[spaces.length - 1] as HTMLElement;
    assertTrue(!!firstSpan);
    assertTrue(!!lastSpan);

    const firstSpanRect = firstSpan.getBoundingClientRect();
    const lastSpanRect = lastSpan.getBoundingClientRect();

    // The last space span should wrap to a line below the first span.
    assertTrue(lastSpanRect.top > firstSpanRect.top);
  });
});

suite('ComposeboxSkills', () => {
  let inputElement: ComposeboxInputElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    inputElement = document.createElement('cr-composebox-input');
    inputElement.composeboxSkillsEnabled = true;
    document.body.appendChild(inputElement);
    await inputElement.updateComplete;
  });

  test('CaretPlacedAtEndWhenSkillsEnabledAndInputChanges', async () => {
    const input = inputElement.shadowRoot.querySelector<HTMLElement>('#input')!;
    const caret = inputElement.shadowRoot.querySelector<HTMLElement>('#caret');
    const mirror =
        inputElement.shadowRoot.querySelector<HTMLElement>('#mirror');
    assertTrue(!!input);
    assertTrue(!!caret);
    assertTrue(!!mirror);

    input.focus();
    await inputElement.updateComplete;

    inputElement.input = 'hello world';
    await inputElement.updateComplete;
    await microtasksFinished();

    assertEquals(11, inputElement.getSelectionEnd());

    const lastSpan = mirror.childNodes[10] as HTMLElement;
    assertTrue(!!lastSpan);
    assertEquals('--cursor-char', lastSpan.style.anchorName);
    assertFalse(caret.classList.contains('at-start'));
  });

  test('MirrorReplicatesChips', async () => {
    inputElement.composeboxSkillsEnabled = true;
    await inputElement.updateComplete;

    inputElement.insertSkillChip({id: 'chip1', text: '/Search'});
    await inputElement.updateComplete;

    const chip =
        inputElement.shadowRoot.querySelector<HTMLElement>(
                                   '#input')!.querySelector(`.${CHIP_CLASS}`);
    assertTrue(isVisible(chip));

    const mirror =
        inputElement.shadowRoot.querySelector<HTMLElement>('#mirror');
    assertTrue(!!mirror);

    const mirrorChip = mirror.querySelector(`.${CHIP_CLASS}`);
    assertTrue(!!mirrorChip);

    // Mirror chip contains character spans for '/Search'.
    const charSpans = mirrorChip.querySelectorAll('span');
    assertEquals(7, charSpans.length);
    assertEquals('/', charSpans[0]!.textContent);
    assertEquals('S', charSpans[1]!.textContent);
  });

  test('CaretAnchorsToLastCharacterSpanWithChip', async () => {
    inputElement.composeboxSkillsEnabled = true;
    await inputElement.updateComplete;

    const inputDiv =
        inputElement.shadowRoot.querySelector<HTMLElement>('#input')!;
    inputElement.insertSkillChip({id: 'chip1', text: '/Translate'});
    await inputElement.updateComplete;

    const chip = inputDiv.querySelector(`.${CHIP_CLASS}`);
    assertTrue(isVisible(chip));

    inputDiv.focus();
    const range = document.createRange();
    range.selectNodeContents(inputDiv);
    range.collapse(/*toStart=*/ false);
    const sel = window.getSelection();
    sel?.removeAllRanges();
    sel?.addRange(range);

    inputDiv.dispatchEvent(new Event('keyup', {bubbles: true}));
    await inputElement.updateComplete;

    const mirror =
        inputElement.shadowRoot.querySelector<HTMLElement>('#mirror');
    assertTrue(!!mirror);

    const charSpans = mirror.querySelectorAll(`span:not(.${CHIP_CLASS})`);
    assertEquals(11, charSpans.length);

    // The last character span is the trailing space (offset 11), which should
    // have the anchor.
    const lastCharSpan = charSpans[10] as HTMLElement;
    assertEquals(NON_BREAKING_SPACE, lastCharSpan.textContent);
    assertEquals('--cursor-char', lastCharSpan.style.anchorName);

    // The second-to-last span ('e') should NOT have the anchor.
    const secondToLastCharSpan = charSpans[9] as HTMLElement;
    assertEquals('e', secondToLastCharSpan.textContent);
    assertEquals('', secondToLastCharSpan.style.anchorName);
  });

  test('InputWithNewlinePreservedWhenSkillsEnabled', async () => {
    inputElement.composeboxSkillsEnabled = true;
    await inputElement.updateComplete;

    const inputDiv =
        inputElement.shadowRoot.querySelector<HTMLElement>('#input')!;
    inputDiv.textContent = '\n';
    inputDiv.dispatchEvent(new Event('input', {bubbles: true}));
    await inputElement.updateComplete;

    assertEquals('\n', inputElement.input);
    assertEquals(1, inputDiv.childNodes.length);
  });

  test('ResetCaretAnchorsToFirstSpanWithChipWhenSkillsEnabled', async () => {
    inputElement.composeboxSkillsEnabled = true;
    await inputElement.updateComplete;

    const inputDiv =
        inputElement.shadowRoot.querySelector<HTMLElement>('#input')!;
    const chipEl = createChipElement({id: 'chip1', text: '/Search'});
    inputDiv.appendChild(chipEl);
    inputElement.input = '/Search';
    await inputElement.updateComplete;

    const caret = inputElement.shadowRoot.querySelector<HTMLElement>('#caret');
    const mirror =
        inputElement.shadowRoot.querySelector<HTMLElement>('#mirror');
    assertTrue(!!caret);
    assertTrue(!!mirror);

    inputElement.resetCaret();

    const firstCharSpan =
        mirror.querySelector<HTMLElement>(`span:not(.${CHIP_CLASS})`);
    assertTrue(!!firstCharSpan);
    assertEquals('/', firstCharSpan.textContent);
    assertEquals('--cursor-char', firstCharSpan.style.anchorName);
    assertTrue(caret.classList.contains('at-start'));
  });

  test(
      'InputPropertyChangeOverwritesExistingChipsWhenSkillsEnabled',
      async () => {
        inputElement.composeboxSkillsEnabled = true;
        await inputElement.updateComplete;

        const inputDiv =
            inputElement.shadowRoot.querySelector<HTMLElement>('#input')!;
        const chipEl = createChipElement({id: 'chip1', text: '/Search'});
        inputDiv.appendChild(chipEl);
        inputElement.input = '/Search';
        await inputElement.updateComplete;
        assertEquals(1, inputDiv.querySelectorAll(`.${CHIP_CLASS}`).length);

        inputDiv.focus();
        inputElement.input = 'new query';
        await inputElement.updateComplete;
        await microtasksFinished();

        assertEquals(0, inputDiv.querySelectorAll(`.${CHIP_CLASS}`).length);
        assertEquals('new query', inputDiv.innerText);
        assertEquals(9, inputElement.getSelectionEnd());
      });

  test(
      'InputPropertyClearedRemovesExistingChipsWhenSkillsEnabled',
      async () => {
        inputElement.composeboxSkillsEnabled = true;
        await inputElement.updateComplete;

        const inputDiv =
            inputElement.shadowRoot.querySelector<HTMLElement>('#input')!;
        const chipEl = createChipElement({id: 'chip1', text: '/Search'});
        inputDiv.appendChild(chipEl);
        inputElement.input = '/Search';
        await inputElement.updateComplete;
        assertEquals(1, inputDiv.querySelectorAll(`.${CHIP_CLASS}`).length);

        inputElement.input = '';
        await inputElement.updateComplete;

        assertEquals(0, inputDiv.querySelectorAll(`.${CHIP_CLASS}`).length);
        assertEquals(0, inputDiv.childNodes.length);
      });

  test('SanitizationPipelinePreservesValidChip', () => {
    const chipEl = createChipElement({
      id: 'chip1',
      text: '/Search',
      description: 'Search the web',
      emoji: '🔍',
      iconUrl: 'https://example.com/icon.png',
    });
    const policy = getChipPolicyForTesting();
    assertTrue(!!policy);
    const rawHtml = `${chipEl.outerHTML}&nbsp;`;
    const trustedHtml = policy.createHTML(rawHtml);

    const tempDiv = document.createElement('div');
    tempDiv.innerHTML = trustedHtml as unknown as string;

    const chip = tempDiv.querySelector<HTMLElement>(`.${CHIP_CLASS}`);
    assertTrue(!!chip);
    assertEquals('chip1', chip.dataset['chipId']);
    assertEquals('/Search', chip.dataset['chipText']);
    assertEquals('🔍', chip.dataset['chipEmoji']);
    assertEquals('https://example.com/icon.png', chip.dataset['chipIconUrl']);
    assertEquals('false', chip.getAttribute('contenteditable'));
    assertEquals('-1', chip.getAttribute('tabindex'));
    assertEquals('Search the web', chip.getAttribute('title'));

    const label = chip.querySelector(`.${CHIP_LABEL_CLASS}`);
    assertTrue(!!label);
    assertEquals('/Search', label.textContent);

    // Falls back to `chip.text` when `description` is omitted.
    const fallbackChipEl = createChipElement({text: '/Search'});
    assertEquals('/Search', fallbackChipEl.getAttribute('title'));
  });

  test('SanitizationPipelineStripsDisallowedTagsAndAttributes', () => {
    const policy = getChipPolicyForTesting();
    assertTrue(!!policy);

    // Use a data URI to comply with WebUI img-src CSP.
    const dummyIconUrl =
        'data:image/gif;base64,R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7';

    // Disallowed elements (<script>, <iframe>) and attributes (style,
    // invalid-attr) are stripped, while allowed elements and
    // attributes are preserved.
    // NOTE: Inline event handlers (onclick, onerror) are intentionally excluded
    // from this test string because setHTML() triggers a CSP violation report
    // when parsing them, which intentionally crashes the WebUI test runner.
    const maliciousHtml =
        '<script>console.log("xss")</script><iframe src="about:blank"></iframe>' +
        '<span class="aim-chip" style="color: red;" ' +
        'data-chip-text="valid" invalid-attr="bad">' +
        '<img src="' + dummyIconUrl + '" class="chip-icon" alt="icon" ' +
        'style="display:none">' +
        '</span>';

    const trustedHtml = policy.createHTML(maliciousHtml);

    const tempDiv = document.createElement('div');
    tempDiv.innerHTML = trustedHtml as unknown as string;

    assertEquals(0, tempDiv.querySelectorAll('script').length);
    assertEquals(0, tempDiv.querySelectorAll('iframe').length);

    const span = tempDiv.querySelector('span');
    assertTrue(!!span);
    assertFalse(span.hasAttribute('style'));
    assertFalse(span.hasAttribute('invalid-attr'));
    assertEquals('aim-chip', span.getAttribute('class'));
    assertEquals('valid', span.getAttribute('data-chip-text'));

    const img = tempDiv.querySelector('img');
    assertTrue(!!img);
    assertFalse(img.hasAttribute('style'));
    assertEquals(dummyIconUrl, img.getAttribute('src'));
    assertEquals('chip-icon', img.getAttribute('class'));
    assertEquals('icon', img.getAttribute('alt'));
  });

  test(
      'InsertingChipAtStartPreservedWhenInputPropertyUpdatedWithRegularSpace',
      async () => {
        inputElement.composeboxSkillsEnabled = true;
        await inputElement.updateComplete;

        inputElement.insertSkillChip({id: 'chip1', text: '/Search'});
        await inputElement.updateComplete;

        const inputDiv =
            inputElement.shadowRoot.querySelector<HTMLElement>('#input')!;
        let chip = inputDiv.querySelector(`.${CHIP_CLASS}`);
        assertTrue(isVisible(chip));

        // When autocomplete selects the first match or updates input with
        // a regular space ('\u0020'), the chip should not be stripped.
        inputElement.input = '/Search ';
        await inputElement.updateComplete;

        chip = inputDiv.querySelector(`.${CHIP_CLASS}`);
        assertTrue(isVisible(chip));
        assertEquals('/Search ', inputElement.input);
      });

  test(
      'InsertingChipAtStartPreservedWhenInputPropertyUpdatedWithTrimmedText',
      async () => {
        inputElement.composeboxSkillsEnabled = true;
        await inputElement.updateComplete;

        inputElement.insertSkillChip({id: 'chip1', text: '/Search'});
        await inputElement.updateComplete;

        const inputDiv =
            inputElement.shadowRoot.querySelector<HTMLElement>('#input')!;
        let chip = inputDiv.querySelector(`.${CHIP_CLASS}`);
        assertTrue(isVisible(chip));

        // When autocomplete returns a match whose fillIntoEdit omits the
        // trailing space (e.g. '/Search'), the chip should still be preserved.
        inputElement.input = '/Search';
        await inputElement.updateComplete;

        chip = inputDiv.querySelector(`.${CHIP_CLASS}`);
        assertTrue(isVisible(chip));
      });

  test('InsertingChipAtStartOfExistingTextPreserved', async () => {
    inputElement.composeboxSkillsEnabled = true;
    await inputElement.updateComplete;

    inputElement.input = 'hello';
    await inputElement.updateComplete;

    const inputDiv =
        inputElement.shadowRoot.querySelector<HTMLElement>('#input')!;
    // Place selection at the start (offset 0).
    const range = document.createRange();
    range.setStart(inputDiv.firstChild!, 0);
    range.collapse(true);
    const sel = window.getSelection();
    sel?.removeAllRanges();
    sel?.addRange(range);

    inputElement.insertSkillChip({id: 'chip1', text: '/Search'});
    await inputElement.updateComplete;

    let chip = inputDiv.querySelector(`.${CHIP_CLASS}`);
    assertTrue(isVisible(chip));

    inputElement.input = '/Search hello';
    await inputElement.updateComplete;

    chip = inputDiv.querySelector(`.${CHIP_CLASS}`);
    assertTrue(isVisible(chip));
  });

  test('CopyAndPasteChipWithUndoRedo', async () => {
    inputElement.composeboxSkillsEnabled = true;
    await inputElement.updateComplete;

    const inputDiv =
        inputElement.shadowRoot.querySelector<HTMLElement>('#input')!;
    inputElement.insertSkillChip({
      id: 'chip1',
      text: '/Search',
      emoji: '🔍',
      iconUrl: 'https://example.com/icon.png',
    });
    await inputElement.updateComplete;

    // Select only the chip element (without its trailing space) and copy.
    const chipEl = inputDiv.querySelector(`.${CHIP_CLASS}`)!;
    const range = document.createRange();
    range.selectNode(chipEl);
    const sel = window.getSelection();
    sel?.removeAllRanges();
    sel?.addRange(range);

    const clipboardData = new DataTransfer();
    inputDiv.dispatchEvent(new ClipboardEvent('copy', {
      clipboardData,
      bubbles: true,
      cancelable: true,
    }));

    assertEquals('/Search ', clipboardData.getData('text/plain'));
    assertTrue(clipboardData.getData('text/html').includes(CHIP_CLASS));
    assertTrue(clipboardData.getData('text/html').endsWith('&nbsp;'));

    // Clear input and paste the copied content.
    inputElement.input = '';
    await inputElement.updateComplete;
    assertEquals(0, inputDiv.querySelectorAll(`.${CHIP_CLASS}`).length);

    inputDiv.dispatchEvent(new ClipboardEvent('paste', {
      clipboardData,
      bubbles: true,
      cancelable: true,
    }));
    await inputElement.updateComplete;

    let pastedChip = inputDiv.querySelector<HTMLElement>(`.${CHIP_CLASS}`);
    assertTrue(!!pastedChip);
    assertEquals('chip1', pastedChip.dataset['chipId']);
    assertEquals('/Search', pastedChip.dataset['chipText']);
    assertEquals('🔍', pastedChip.dataset['chipEmoji']);
    assertEquals(
        'https://example.com/icon.png', pastedChip.dataset['chipIconUrl']);
    assertEquals('/Search ', inputElement.input);

    // Undo should remove the pasted chip.
    document.execCommand('undo');
    await inputElement.updateComplete;
    assertEquals(0, inputDiv.querySelectorAll(`.${CHIP_CLASS}`).length);
    assertEquals('', inputElement.input);

    // Redo should restore the pasted chip.
    document.execCommand('redo');
    await inputElement.updateComplete;
    pastedChip = inputDiv.querySelector<HTMLElement>(`.${CHIP_CLASS}`);
    assertTrue(!!pastedChip);
    assertEquals('/Search ', inputElement.input);
  });

  test('PasteSanitizesMaliciousHtmlAndPreservesChip', async () => {
    inputElement.composeboxSkillsEnabled = true;
    await inputElement.updateComplete;

    // Inline event handlers (e.g. onclick) are excluded because setHTML()
    // triggers a CSP violation report when parsing them, which crashes the
    // WebUI test runner.
    const inputDiv =
        inputElement.shadowRoot.querySelector<HTMLElement>('#input')!;
    const clipboardData = new DataTransfer();
    clipboardData.setData(
        'text/html',
        '<script>console.log("xss")</script><iframe src="about:blank"></iframe>' +
            '<span class="aim-chip" contenteditable="false" tabindex="-1" ' +
            'style="color: red;" invalid-attr="bad" ' +
            'data-chip-id="chip1" data-chip-text="/Search">' +
            '<span class="chip-label">/Search</span></span> hello');

    inputDiv.dispatchEvent(new ClipboardEvent('paste', {
      clipboardData,
      bubbles: true,
      cancelable: true,
    }));
    await inputElement.updateComplete;

    assertEquals(0, inputDiv.querySelectorAll('script').length);
    assertEquals(0, inputDiv.querySelectorAll('iframe').length);
    const chip = inputDiv.querySelector<HTMLElement>(`.${CHIP_CLASS}`);
    assertTrue(!!chip);
    assertFalse(chip.hasAttribute('style'));
    assertFalse(chip.hasAttribute('invalid-attr'));
    assertEquals('chip1', chip.dataset['chipId']);
    assertEquals('/Search', chip.dataset['chipText']);
    assertEquals('/Search hello', inputElement.input);
  });
});
