// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/webui_toolbar.js';

import {assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import type {ToolbarChipButtonElement} from 'chrome://webui-toolbar.top-chrome/webui_toolbar.js';

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

  test('ForwardButtonRole', async function() {
    const button = element.$.button;

    // Default is null (not present), keeping the implicit button role.
    assertFalse(button.hasAttribute('role'));

    // Set property
    element.buttonRole = 'img';
    await microtasksFinished();
    assertEquals('img', button.getAttribute('role'));

    // Clear property
    element.buttonRole = null;
    await microtasksFinished();
    assertFalse(button.hasAttribute('role'));
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

  test('CollapsesGapAndLabelWithoutHasLabel', async function() {
    const prefixIcon = document.createElement('div');
    prefixIcon.slot = 'prefix-icon';
    const content = document.createElement('span');
    content.textContent = 'button label';
    element.appendChild(prefixIcon);
    element.appendChild(content);
    element.style.setProperty('--toolbar-chip-icon-label-gap', '8px');
    await microtasksFinished();

    assertEquals('0px', window.getComputedStyle(element.$.button).gap);
    assertEquals('0px', window.getComputedStyle(content).maxWidth);

    element.toggleAttribute('has-label', true);
    await microtasksFinished();

    assertEquals('8px', window.getComputedStyle(element.$.button).gap);
    assertEquals('none', window.getComputedStyle(content).maxWidth);
  });

  function getCornerRadii(el: Element, pseudo?: string) {
    const style = window.getComputedStyle(el, pseudo);
    return {
      topLeft: style.borderTopLeftRadius,
      topRight: style.borderTopRightRadius,
      bottomLeft: style.borderBottomLeftRadius,
      bottomRight: style.borderBottomRightRadius,
    };
  }

  test('DefaultsToFullyRoundedCorners', async function() {
    element.style.setProperty('--toolbar-chip-height', '28px');
    await microtasksFinished();

    assertFalse(element.hasAttribute('flat-edge'));
    const radii = getCornerRadii(element.$.button);
    assertEquals('14px', radii.topLeft);
    assertEquals('14px', radii.topRight);
    assertEquals('14px', radii.bottomLeft);
    assertEquals('14px', radii.bottomRight);
  });

  // The flattened side uses a 2px radius to match Views kSplitFlatEdgetRadius.
  test('FlatEdgeEndFlattensTrailingCorners', async function() {
    element.style.setProperty('--toolbar-chip-height', '28px');
    element.flatEdge = 'end';
    await microtasksFinished();

    let radii = getCornerRadii(element.$.button);
    assertEquals('14px', radii.topLeft);
    assertEquals('14px', radii.bottomLeft);
    assertEquals('2px', radii.topRight);
    assertEquals('2px', radii.bottomRight);

    element.flatEdge = null;
    await microtasksFinished();

    radii = getCornerRadii(element.$.button);
    assertEquals('14px', radii.topLeft);
    assertEquals('14px', radii.bottomLeft);
    assertEquals('14px', radii.topRight);
    assertEquals('14px', radii.bottomRight);
  });

  test('FlatEdgeStartFlattensLeadingCorners', async function() {
    element.style.setProperty('--toolbar-chip-height', '28px');
    element.flatEdge = 'start';
    await microtasksFinished();

    const radii = getCornerRadii(element.$.button);
    assertEquals('2px', radii.topLeft);
    assertEquals('2px', radii.bottomLeft);
    assertEquals('14px', radii.topRight);
    assertEquals('14px', radii.bottomRight);
  });

  test('FlatEdgeMirrorsInRtl', async function() {
    element.style.setProperty('--toolbar-chip-height', '28px');
    element.dir = 'rtl';
    element.flatEdge = 'end';
    await microtasksFinished();

    const radii = getCornerRadii(element.$.button);
    assertEquals('2px', radii.topLeft);
    assertEquals('2px', radii.bottomLeft);
    assertEquals('14px', radii.topRight);
    assertEquals('14px', radii.bottomRight);
  });

  // The hover and pressed highlight layers must follow the split shape, like
  // Views' ToolbarButtonHighlightPathGenerator.
  test('FlatEdgeAppliesToHighlightLayers', async function() {
    element.style.setProperty('--toolbar-chip-height', '28px');
    element.flatEdge = 'end';
    await microtasksFinished();

    const buttonRadii = getCornerRadii(element.$.button);
    for (const pseudo of ['::before', '::after']) {
      const radii = getCornerRadii(element.$.button, pseudo);
      assertEquals(buttonRadii.topLeft, radii.topLeft, pseudo);
      assertEquals(buttonRadii.topRight, radii.topRight, pseudo);
      assertEquals(buttonRadii.bottomLeft, radii.bottomLeft, pseudo);
      assertEquals(buttonRadii.bottomRight, radii.bottomRight, pseudo);
    }
  });
});
