// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/lazy_load.js';

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import type {ToolbarChipButtonElement} from 'chrome://webui-toolbar.top-chrome/lazy_load.js';

suite('ToolbarChipButtonTest', function() {
  let element: ToolbarChipButtonElement;

  setup(async function() {
    document.body.className = '';
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('toolbar-chip-button');
    document.body.appendChild(element);
    await microtasksFinished();
  });

  test('CreatesElement', function() {
    assertEquals('TOOLBAR-CHIP-BUTTON', element.tagName);
  });

  test('FocusVisible', function() {
    const button = element.$.button;

    element.focus();
    assertEquals(button, element.shadowRoot.activeElement);

    // Requesting a focus ring applies it even if the button already has focus
    // without one.
    button.blur();
    button.focus({focusVisible: false});
    assertFalse(button.matches(':focus-visible'));
    element.focus({focusVisible: true});
    assertEquals(button, element.shadowRoot.activeElement);
    assertTrue(button.matches(':focus-visible'));
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

  test('PreservesForcedColorsBehavior', async function() {
    assertEquals('none', window.getComputedStyle(element).forcedColorAdjust);
    assertEquals(
        'none', window.getComputedStyle(element.$.button).forcedColorAdjust);

    element.toggleAttribute('disabled', true);
    await microtasksFinished();
    assertEquals('none', window.getComputedStyle(element).forcedColorAdjust);
  });

  test('PreservesCustomForegroundColorOnMenuOpen', async function() {
    const forcedColors = window.matchMedia('(forced-colors: active)').matches;
    const expectedForcedColors =
        (window as {expectedForcedColors?: boolean}).expectedForcedColors;
    assertEquals(expectedForcedColors, forcedColors);

    element.style.setProperty('--color-toolbar-button-icon', 'rgb(1, 2, 3)');
    element.style.setProperty(
        '--color-toolbar-button-icon-pressed', 'rgb(7, 8, 9)');
    element.style.setProperty('--color-toolbar-button-text', 'rgb(10, 11, 12)');
    element.style.setProperty(
        '--color-toolbar-button-icon-inactive', 'rgb(13, 14, 15)');
    element.style.setProperty('--toolbar-chip-fg-color', 'rgb(20, 30, 40)');
    element.toggleAttribute('is-menu-open', true);
    await microtasksFinished();

    assertEquals(
        forcedColors ? 'rgb(7, 8, 9)' : 'rgb(20, 30, 40)',
        window.getComputedStyle(element).color);
    assertEquals(
        forcedColors ? 'currentColor' : '',
        window.getComputedStyle(element)
            .getPropertyValue('--icon-from-table-color-override')
            .trim());

    element.style.removeProperty('--toolbar-chip-fg-color');
    await microtasksFinished();
    assertEquals(
        forcedColors ? 'rgb(7, 8, 9)' : 'rgb(1, 2, 3)',
        window.getComputedStyle(element).color);

    element.toggleAttribute('has-label', true);
    await microtasksFinished();
    assertEquals(
        forcedColors ? 'rgb(7, 8, 9)' : 'rgb(10, 11, 12)',
        window.getComputedStyle(element).color);

    element.toggleAttribute('is-menu-open', false);
    document.body.classList.add('anchor-highlight');
    await microtasksFinished();
    assertEquals(
        forcedColors ? 'rgb(7, 8, 9)' : 'rgb(10, 11, 12)',
        window.getComputedStyle(element).color);

    element.toggleAttribute('disabled', true);
    await microtasksFinished();
    assertEquals('rgb(13, 14, 15)', window.getComputedStyle(element).color);
  });

  test('FocusRingRemainsVisibleInForcedColors', async function() {
    const forcedColors = window.matchMedia('(forced-colors: active)').matches;
    element.style.setProperty('--cr-focus-outline-color', 'rgb(0, 100, 200)');
    element.toggleAttribute('force-focus-ring', true);
    await microtasksFinished();

    const probe = document.createElement('div');
    probe.style.forcedColorAdjust = 'none';
    probe.style.color = 'ButtonText';
    document.body.appendChild(probe);
    const buttonTextColor = window.getComputedStyle(probe).color;
    probe.remove();

    let style = window.getComputedStyle(element.$.button);
    if (forcedColors) {
      assertEquals('none', style.boxShadow);
      assertEquals('solid', style.outlineStyle);
      assertEquals('2px', style.outlineWidth);
      assertEquals('0px', style.outlineOffset);
      assertNotEquals('rgba(0, 0, 0, 0)', style.outlineColor);
      assertEquals(buttonTextColor, style.outlineColor);

      element.style.setProperty(
          '--color-toolbar-focus-ring', 'rgb(12, 34, 56)');
      await microtasksFinished();
      assertEquals(
          'rgb(12, 34, 56)',
          window.getComputedStyle(element.$.button).outlineColor);
      element.style.removeProperty('--color-toolbar-focus-ring');
      await microtasksFinished();
    } else {
      assertTrue(style.boxShadow.includes('rgb(0, 100, 200)'));
      assertEquals('none', style.outlineStyle);

      element.style.setProperty(
          '--color-toolbar-focus-ring', 'rgb(12, 34, 56)');
      await microtasksFinished();
      assertTrue(window.getComputedStyle(element.$.button)
                     .boxShadow.includes('rgb(12, 34, 56)'));
      element.style.removeProperty('--color-toolbar-focus-ring');
      await microtasksFinished();
    }

    element.toggleAttribute('outset-focus-ring', true);
    await microtasksFinished();

    style = window.getComputedStyle(element.$.button);
    assertEquals('none', style.boxShadow);
    assertEquals('solid', style.outlineStyle);
    assertEquals('2px', style.outlineWidth);
    assertEquals('2px', style.outlineOffset);
    assertEquals(
        forcedColors ? buttonTextColor : 'rgb(0, 100, 200)',
        style.outlineColor);

    element.style.setProperty('--color-toolbar-focus-ring', 'rgb(12, 34, 56)');
    await microtasksFinished();
    assertEquals(
        'rgb(12, 34, 56)',
        window.getComputedStyle(element.$.button).outlineColor);
  });
});
