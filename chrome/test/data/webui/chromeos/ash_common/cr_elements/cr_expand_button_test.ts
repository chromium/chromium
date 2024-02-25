// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/ash/common/cr_elements/cr_expand_button/cr_expand_button.js';

import {CrExpandButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_expand_button/cr_expand_button.js';
import {CrIconButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
// clang-format on

suite('cr-expand-button', function() {
  let button: CrExpandButtonElement;
  let icon: CrIconButtonElement;
  const expandTitle = 'expand title';
  const collapseTitle = 'collapse title';

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    button = document.createElement('cr-expand-button');
    document.body.appendChild(button);
    icon = button.shadowRoot!.querySelector<CrIconButtonElement>('#icon')!;
  });

  test('setting |aria-label| label', () => {
    assertFalse(!!button.ariaLabel);
    assertEquals('label', icon.getAttribute('aria-labelledby'));
    assertEquals(null, icon.getAttribute('aria-label'));
    const ariaLabel = 'aria-label label';
    button.ariaLabel = ariaLabel;
    assertEquals(null, icon.getAttribute('aria-labelledby'));
    assertEquals(ariaLabel, icon.getAttribute('aria-label'));
  });

  test('changing |expanded|', () => {
    button.expandTitle = expandTitle;
    button.collapseTitle = collapseTitle;
    assertFalse(button.expanded);
    assertEquals(expandTitle, button.title);
    assertEquals('false', icon.getAttribute('aria-expanded'));
    assertEquals('cr:expand-more', icon.ironIcon);
    button.expanded = true;
    assertEquals(collapseTitle, button.title);
    assertEquals('true', icon.getAttribute('aria-expanded'));
    assertEquals('cr:expand-less', icon.ironIcon);
  });

  test('changing |disabled|', () => {
    assertFalse(button.disabled);
    assertEquals('false', icon.getAttribute('aria-expanded'));
    assertFalse(icon.disabled);
    button.disabled = true;
    assertFalse(icon.hasAttribute('aria-expanded'));
    assertTrue(icon.disabled);
  });

  // Ensure that the label is marked with aria-hidden="true", so that screen
  // reader focus goes straight to the cr-icon-button.
  test('label aria-hidden', () => {
    const labelId = 'label';
    assertEquals(
        'true',
        button.shadowRoot!.querySelector(`#${labelId}`)!.getAttribute(
            'aria-hidden'));
    assertEquals(labelId, icon.getAttribute('aria-labelledby'));
  });

  test('setting |expand-icon| and |collapse-icon|', () => {
    const expandIconName = 'cr:arrow-drop-down';
    button.setAttribute('expand-icon', expandIconName);
    const collapseIconName = 'cr:arrow-drop-up';
    button.setAttribute('collapse-icon', collapseIconName);

    assertFalse(button.expanded);
    assertEquals(expandIconName, icon.ironIcon);
    button.expanded = true;
    assertEquals(collapseIconName, icon.ironIcon);
  });

  test('setting |expand-title| and |collapse-title|', () => {
    assertFalse(button.expanded);
    button.expandTitle = expandTitle;
    assertEquals(expandTitle, button.title);

    button.click();
    button.collapseTitle = collapseTitle;
    assertEquals(collapseTitle, button.title);
  });
});
