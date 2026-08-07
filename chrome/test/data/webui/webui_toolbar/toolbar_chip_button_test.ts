// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import type {ToolbarChipButtonElement} from 'chrome://webui-toolbar.top-chrome/app.js';

suite('ToolbarChipButtonTest', function() {
  let element: ToolbarChipButtonElement;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('toolbar-chip-button');
    document.body.appendChild(element);
    await microtasksFinished();
  });

  test('CreatesElement', function() {
    assertEquals('TOOLBAR-CHIP-BUTTON', element.tagName);
  });

  test('ForwardAriaLabel', async function() {
    const button = element.$.button;

    // Default is empty
    assertEquals('', button.getAttribute('aria-label'));

    // Set property
    element.ariaLabel = 'new-aria-label';
    await microtasksFinished();
    assertEquals('new-aria-label', button.getAttribute('aria-label'));
  });

  test('ForwardAriaHasPopup', async function() {
    const button = element.$.button;

    // Default is null (not present)
    assertFalse(button.hasAttribute('aria-haspopup'));

    // Set property
    element.ariaHasPopup = 'dialog';
    await microtasksFinished();
    assertEquals('dialog', button.getAttribute('aria-haspopup'));

    // Clear property
    element.ariaHasPopup = null;
    await microtasksFinished();
    assertFalse(button.hasAttribute('aria-haspopup'));
  });

  test('ForwardAriaExpanded', async function() {
    const button = element.$.button;

    // Default is null (not present)
    assertFalse(button.hasAttribute('aria-expanded'));

    // Set property
    element.ariaExpanded = 'false';
    await microtasksFinished();
    assertEquals('false', button.getAttribute('aria-expanded'));

    // Clear property
    element.ariaExpanded = null;
    await microtasksFinished();
    assertFalse(button.hasAttribute('aria-expanded'));
  });


  test('ForwardTooltip', async function() {
    const button = element.$.button;

    // Default is empty
    assertEquals('', button.getAttribute('title'));

    // Set property 'tooltip'
    element.tooltip = 'new-title';
    await microtasksFinished();
    assertEquals('new-title', button.getAttribute('title'));
  });

  test('SlotProjection', async function() {
    const prefixIcon = document.createElement('div');
    prefixIcon.slot = 'prefix-icon';
    prefixIcon.id = 'prefix';

    const suffixIcon = document.createElement('div');
    suffixIcon.slot = 'suffix-icon';
    suffixIcon.id = 'suffix';

    const content = document.createElement('span');
    content.textContent = 'button label';
    content.id = 'content';

    element.appendChild(prefixIcon);
    element.appendChild(content);
    element.appendChild(suffixIcon);
    await microtasksFinished();

    const prefixSlot = element.shadowRoot.querySelector<HTMLSlotElement>(
        'slot[name="prefix-icon"]')!;
    const defaultSlot =
        element.shadowRoot.querySelector<HTMLSlotElement>('slot:not([name])')!;
    const suffixSlot = element.shadowRoot.querySelector<HTMLSlotElement>(
        'slot[name="suffix-icon"]')!;

    assertEquals(1, prefixSlot.assignedNodes().length);
    assertEquals(prefixIcon, prefixSlot.assignedNodes()[0]);

    assertEquals(1, defaultSlot.assignedNodes().length);
    assertEquals(content, defaultSlot.assignedNodes()[0]);

    assertEquals(1, suffixSlot.assignedNodes().length);
    assertEquals(suffixIcon, suffixSlot.assignedNodes()[0]);
  });
});
