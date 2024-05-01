// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_progress/cr_progress.js';

import type {CrProgressElement} from 'chrome://resources/cr_elements/cr_progress/cr_progress.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('cr-progress', function() {
  let progress: CrProgressElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    progress = document.createElement('cr-progress');
    document.body.appendChild(progress);
  });

  test('sets correct aria attributes', () => {
    assertEquals('100', progress.getAttribute('aria-valuemax'));
    assertEquals('0', progress.getAttribute('aria-valuemin'));
    assertEquals('0', progress.getAttribute('aria-valuenow'));
    assertEquals('progressbar', progress.getAttribute('role'));
    assertEquals('false', progress.getAttribute('aria-disabled'));
  });

  test('clamps and updates style', async () => {
    progress.min = 10;
    await progress.updateComplete;
    assertEquals(10, progress.value);
    assertEquals('scaleX(0)', progress.$.primaryProgress.style.transform);

    progress.value = 50;
    progress.max = 20;
    await progress.updateComplete;
    assertEquals(20, progress.value);
    assertEquals('scaleX(1)', progress.$.primaryProgress.style.transform);

    // Set to halfway.
    progress.value = 15;
    await progress.updateComplete;
    assertEquals('scaleX(0.5)', progress.$.primaryProgress.style.transform);

    // Validate aria attributes
    assertEquals('20', progress.getAttribute('aria-valuemax'));
    assertEquals('10', progress.getAttribute('aria-valuemin'));
    assertEquals('15', progress.getAttribute('aria-valuenow'));
  });

  test('indeterminate mode', async () => {
    assertEquals('0', progress.getAttribute('aria-valuenow'));
    assertEquals(
        'none',
        (progress.$.primaryProgress.computedStyleMap().get('animation-name') as
         CSSKeywordValue)
            .value);

    progress.indeterminate = true;
    await progress.updateComplete;

    // No valuenow in indeterminate mode.
    assertFalse(progress.hasAttribute('aria-valuenow'));
    assertEquals(
        'indeterminate-bar',
        (progress.$.primaryProgress.computedStyleMap().get('animation-name') as
         CSSKeywordValue)
            .value);

    // Disabling turns off the animation.
    progress.disabled = true;
    await progress.updateComplete;
    assertEquals('true', progress.getAttribute('aria-disabled'));
    assertEquals(
        'none',
        (progress.$.primaryProgress.computedStyleMap().get('animation-name') as
         CSSKeywordValue)
            .value);
  });

  test('reflects to attribute', async () => {
    assertFalse(progress.hasAttribute('disabled'));
    assertFalse(progress.hasAttribute('indeterminate'));

    progress.disabled = true;
    await progress.updateComplete;
    assertTrue(progress.hasAttribute('disabled'));
    assertFalse(progress.hasAttribute('indeterminate'));

    progress.indeterminate = true;
    await progress.updateComplete;
    assertTrue(progress.hasAttribute('disabled'));
    assertTrue(progress.hasAttribute('indeterminate'));

    progress.toggleAttribute('indeterminate', false);
    progress.toggleAttribute('disabled', false);
    await progress.updateComplete;
    assertFalse(progress.disabled);
    assertFalse(progress.indeterminate);
  });
});
