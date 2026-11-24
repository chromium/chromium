// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';

import {CrExpandButtonDirection} from 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import type {CrExpandButtonElement} from 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';
// clang-format on

suite('cr-expand-button', function() {
  let button: CrExpandButtonElement;
  let icon: CrIconButtonElement;
  const expandTitle = 'expand title';
  const collapseTitle = 'collapse title';

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    button = document.createElement('cr-expand-button');
    document.body.appendChild(button);
    icon = button.$.icon;
    await button.updateComplete;
  });

  test('setting |aria-label| label', async () => {
    assertFalse(!!button.ariaLabel);
    assertEquals('label', icon.getAttribute('aria-labelledby'));
    assertEquals(null, icon.getAttribute('aria-label'));
    const ariaLabel = 'aria-label label';
    button.ariaLabel = ariaLabel;
    await button.updateComplete;
    assertEquals(null, icon.getAttribute('aria-labelledby'));
    assertEquals(ariaLabel, icon.getAttribute('aria-label'));
  });

  test('setting |aria-description|', async () => {
    assertEquals(null, icon.getAttribute('aria-description'));
    const ariaDescription = 'aria-description text';
    button.ariaDescription = ariaDescription;
    await button.updateComplete;
    assertEquals(ariaDescription, icon.getAttribute('aria-description'));
    button.ariaDescription = null;
    await button.updateComplete;
    assertEquals(null, icon.getAttribute('aria-description'));
  });

  const CARDINAL_DIRECTIONS: Array<[CrExpandButtonDirection, number, number]> =
      [
        [CrExpandButtonDirection.UP, 0, -1],
        [CrExpandButtonDirection.DOWN, 0, 1],
        [CrExpandButtonDirection.LEFT, -1, 0],
        [CrExpandButtonDirection.RIGHT, 1, 0],
      ];

  async function getIconPointingDirection(): Promise<CrExpandButtonDirection> {
    await icon.updateComplete;
    const crIcon = icon.shadowRoot.querySelector('cr-icon');
    assertTrue(!!crIcon);
    const iconMatrix = new DOMMatrix(getComputedStyle(icon).transform);
    const crIconMatrix = new DOMMatrix(getComputedStyle(crIcon).transform);
    const tip =
        iconMatrix.multiply(crIconMatrix).transformPoint(new DOMPoint(0, 1));
    const match = CARDINAL_DIRECTIONS.find(
        ([, x, y]) => Math.abs(tip.x - x) < 1e-3 && Math.abs(tip.y - y) < 1e-3);
    assertTrue(!!match, `Unexpected pointing vector (${tip.x}, ${tip.y})`);
    return match[0];
  }

  test('changing |expanded|', async () => {
    button.expandTitle = expandTitle;
    button.collapseTitle = collapseTitle;
    await button.updateComplete;
    assertFalse(button.expanded);
    assertEquals(expandTitle, button.title);
    assertEquals('false', icon.getAttribute('aria-expanded'));
    assertEquals('cr:keyboard-arrow-down', icon.ironIcon);
    assertEquals('none', getComputedStyle(icon).transform);
    assertEquals(
        CrExpandButtonDirection.DOWN, await getIconPointingDirection());
    button.expanded = true;
    await button.updateComplete;
    assertEquals(collapseTitle, button.title);
    assertEquals('true', icon.getAttribute('aria-expanded'));
    assertEquals('cr:keyboard-arrow-down', icon.ironIcon);
    assertEquals('none', getComputedStyle(icon).transform);
    assertEquals(CrExpandButtonDirection.UP, await getIconPointingDirection());
  });

  test('expanded-changed event fires', async () => {
    let whenFired = eventToPromise<CustomEvent<{value: boolean}>>(
        'expanded-changed', button);
    button.expanded = true;
    let event = await whenFired;
    assertTrue(event.detail.value);

    whenFired = eventToPromise<CustomEvent<{value: boolean}>>(
        'expanded-changed', button);
    button.expanded = false;
    event = await whenFired;
    assertFalse(event.detail.value);
  });

  test('changing |disabled|', async () => {
    assertFalse(button.disabled);
    assertEquals('false', icon.getAttribute('aria-expanded'));
    assertFalse(icon.disabled);
    button.disabled = true;
    await button.updateComplete;
    assertEquals('false', icon.getAttribute('aria-expanded'));
    assertTrue(icon.disabled);
  });

  // Ensure that the label is marked with aria-hidden="true", so that screen
  // reader focus goes straight to the cr-icon-button.
  test('label aria-hidden', () => {
    const labelId = 'label';
    assertEquals(
        'true',
        button.shadowRoot.querySelector(`#${labelId}`)!.getAttribute(
            'aria-hidden'));
    assertEquals(labelId, icon.getAttribute('aria-labelledby'));
  });

  test('setting |expand-direction| and |collapse-direction|', async () => {
    button.setAttribute('expand-direction', CrExpandButtonDirection.RIGHT);
    button.setAttribute('collapse-direction', CrExpandButtonDirection.DOWN);
    await button.updateComplete;

    assertFalse(button.expanded);
    assertEquals(CrExpandButtonDirection.RIGHT, button.expandDirection);
    assertEquals(CrExpandButtonDirection.DOWN, button.collapseDirection);
    assertEquals(
        CrExpandButtonDirection.RIGHT, await getIconPointingDirection());

    button.expanded = true;
    await button.updateComplete;
    assertEquals(
        CrExpandButtonDirection.DOWN, await getIconPointingDirection());

    button.expandDirection = CrExpandButtonDirection.LEFT;
    button.collapseDirection = CrExpandButtonDirection.UP;
    await button.updateComplete;
    assertEquals(CrExpandButtonDirection.UP, await getIconPointingDirection());

    button.expanded = false;
    await button.updateComplete;
    assertEquals(
        CrExpandButtonDirection.LEFT, await getIconPointingDirection());
  });

  test('horizontal directions mirror in RTL', async () => {
    const rtlContainer = document.createElement('div');
    rtlContainer.dir = 'rtl';
    rtlContainer.appendChild(button);
    document.body.appendChild(rtlContainer);

    button.expandDirection = CrExpandButtonDirection.RIGHT;
    button.collapseDirection = CrExpandButtonDirection.DOWN;
    await button.updateComplete;
    assertFalse(button.expanded);
    assertEquals(
        CrExpandButtonDirection.LEFT, await getIconPointingDirection());

    button.expanded = true;
    await button.updateComplete;
    assertEquals(
        CrExpandButtonDirection.DOWN, await getIconPointingDirection());

    button.expandDirection = CrExpandButtonDirection.LEFT;
    button.collapseDirection = CrExpandButtonDirection.UP;
    await button.updateComplete;
    assertEquals(CrExpandButtonDirection.UP, await getIconPointingDirection());

    button.expanded = false;
    await button.updateComplete;
    assertEquals(
        CrExpandButtonDirection.RIGHT, await getIconPointingDirection());

    rtlContainer.dir = 'ltr';
    assertEquals(
        CrExpandButtonDirection.LEFT, await getIconPointingDirection());
  });

  test('setting |expand-title| and |collapse-title|', async () => {
    assertFalse(button.expanded);
    button.expandTitle = expandTitle;
    await button.updateComplete;
    assertEquals(expandTitle, button.title);

    button.click();
    button.collapseTitle = collapseTitle;
    await button.updateComplete;
    assertEquals(collapseTitle, button.title);
  });

  test('no tooltip', async () => {
    assertEquals(undefined, button.expandTitle);
    assertEquals(undefined, button.collapseTitle);

    await button.updateComplete;
    assertFalse(button.expanded);
    assertEquals('', button.title);

    button.click();
    await button.updateComplete;
    assertTrue(button.expanded);
    assertEquals('', button.title);
  });

  test('tabIndex propagation', async () => {
    assertEquals(0, button.tabIndex);
    assertEquals('0', icon.getAttribute('tabindex'));

    button.tabIndex = -1;
    await button.updateComplete;
    assertEquals('-1', icon.getAttribute('tabindex'));

    button.tabIndex = 0;
    await button.updateComplete;
    assertEquals('0', icon.getAttribute('tabindex'));
  });
});
