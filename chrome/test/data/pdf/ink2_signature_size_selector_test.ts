// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {SignatureSizeSelectorElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('Ink2SignatureSizeSelector', function() {
  let sizeSelector: SignatureSizeSelectorElement;

  setup(function() {
    document.body.innerHTML = '';
    sizeSelector = document.createElement('signature-size-selector');
    document.body.appendChild(sizeSelector);
  });

  async function clickAndAssertSize(button: HTMLElement, expectedSize: number) {
    const whenChanged = eventToPromise<CustomEvent<{value: number}>>(
        'current-size-changed', sizeSelector);
    button.click();
    const event = await whenChanged;
    assertEquals(expectedSize, event.detail.value);
    assertEquals(expectedSize, sizeSelector.currentSize);
    assertEquals(
        `${expectedSize}%`, sizeSelector.$.percentage.textContent.trim());
  }

  // Test that the size can be decreased and increased by 10%.
  test('SelectSize', async function() {
    assertEquals(100, sizeSelector.currentSize);
    assertEquals('100%', sizeSelector.$.percentage.textContent.trim());
    assertEquals('polite', sizeSelector.$.percentage.getAttribute('aria-live'));
    assertEquals('true', sizeSelector.$.percentage.getAttribute('aria-atomic'));
    assertFalse(sizeSelector.$.decrease.disabled);
    assertFalse(sizeSelector.$.increase.disabled);

    await clickAndAssertSize(sizeSelector.$.decrease, 90);
    assertFalse(sizeSelector.$.decrease.disabled);
    assertFalse(sizeSelector.$.increase.disabled);

    await clickAndAssertSize(sizeSelector.$.increase, 100);
    assertFalse(sizeSelector.$.decrease.disabled);
    assertFalse(sizeSelector.$.increase.disabled);
  });

  // Test that non-multiple sizes snap to the nearest multiple of 10%.
  test('SnapToStep', async function() {
    sizeSelector.currentSize = 35;
    await microtasksFinished();
    await clickAndAssertSize(sizeSelector.$.decrease, 30);

    sizeSelector.currentSize = 35;
    await microtasksFinished();
    await clickAndAssertSize(sizeSelector.$.increase, 40);
  });

  // Test that the size is clamped at the minimum size (25%).
  test('MinSize', async function() {
    sizeSelector.currentSize = 30;
    await microtasksFinished();

    await clickAndAssertSize(sizeSelector.$.decrease, 25);
    assertTrue(sizeSelector.$.decrease.disabled);
    assertFalse(sizeSelector.$.increase.disabled);

    await clickAndAssertSize(sizeSelector.$.increase, 30);
    assertFalse(sizeSelector.$.decrease.disabled);
    assertFalse(sizeSelector.$.increase.disabled);
  });

  // Test that the size is clamped at the maximum size (500%).
  test('MaxSize', async function() {
    sizeSelector.currentSize = 490;
    await microtasksFinished();

    await clickAndAssertSize(sizeSelector.$.increase, 500);
    assertFalse(sizeSelector.$.decrease.disabled);
    assertTrue(sizeSelector.$.increase.disabled);

    await clickAndAssertSize(sizeSelector.$.decrease, 490);
    assertFalse(sizeSelector.$.decrease.disabled);
    assertFalse(sizeSelector.$.increase.disabled);
  });
});
