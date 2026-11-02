// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://tabstrip-ntb.top-chrome/app.js';

import type {TabstripNtbAppElement} from 'chrome://tabstrip-ntb.top-chrome/app.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('NewTabButtonTest', function() {
  let app: TabstripNtbAppElement;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('tabstrip-ntb-app');
    document.body.appendChild(app);
    await app.updateComplete;
  });

  test('ClicksNewTabButton', async function() {
    const newTabButton = app.shadowRoot.querySelector('new-tab-button');
    assertTrue(!!newTabButton);
    await newTabButton.updateComplete;
    const button =
        newTabButton.shadowRoot.querySelector<HTMLElement>('cr-icon-button');
    assertTrue(!!button);
    button.click();
  });

  test('ShowsHoverOnlyForMouseAndPenPointers', async function() {
    const newTabButton = app.shadowRoot.querySelector('new-tab-button');
    assertTrue(!!newTabButton);
    await newTabButton.updateComplete;
    const button =
        newTabButton.shadowRoot.querySelector<HTMLElement>('cr-icon-button');
    assertTrue(!!button);

    assertFalse(newTabButton.hovered);
    assertFalse(newTabButton.hasAttribute('hovered'));
    assertEquals('rgba(0, 0, 0, 0)', getComputedStyle(button).backgroundColor);

    // Touch enter + down must not show hover.
    button.dispatchEvent(new PointerEvent('pointerenter', {
      pointerType: 'touch',
    }));
    await newTabButton.updateComplete;
    assertFalse(newTabButton.hovered);
    assertEquals('rgba(0, 0, 0, 0)', getComputedStyle(button).backgroundColor);

    button.dispatchEvent(new PointerEvent('pointerdown', {
      pointerType: 'touch',
    }));
    await newTabButton.updateComplete;
    assertFalse(newTabButton.hovered);

    // Mouse enter shows hover; mouse leave clears it.
    button.dispatchEvent(new PointerEvent('pointerenter', {
      pointerType: 'mouse',
    }));
    await newTabButton.updateComplete;
    assertTrue(newTabButton.hovered);
    assertTrue(newTabButton.hasAttribute('hovered'));
    assertNotEquals(
        'rgba(0, 0, 0, 0)', getComputedStyle(button).backgroundColor);

    button.dispatchEvent(new PointerEvent('pointerleave', {
      pointerType: 'mouse',
    }));
    await newTabButton.updateComplete;
    assertFalse(newTabButton.hovered);
    assertFalse(newTabButton.hasAttribute('hovered'));
    assertEquals('rgba(0, 0, 0, 0)', getComputedStyle(button).backgroundColor);

    // Pen enter shows hover; touch pointerdown or cancel clears it.
    button.dispatchEvent(new PointerEvent('pointerenter', {
      pointerType: 'pen',
    }));
    await newTabButton.updateComplete;
    assertTrue(newTabButton.hovered);

    button.dispatchEvent(new PointerEvent('pointerdown', {
      pointerType: 'touch',
    }));
    await newTabButton.updateComplete;
    assertFalse(newTabButton.hovered);

    button.dispatchEvent(new PointerEvent('pointerenter', {
      pointerType: 'mouse',
    }));
    await newTabButton.updateComplete;
    assertTrue(newTabButton.hovered);

    button.dispatchEvent(new PointerEvent('pointercancel', {
      pointerType: 'mouse',
    }));
    await newTabButton.updateComplete;
    assertFalse(newTabButton.hovered);
  });
});
