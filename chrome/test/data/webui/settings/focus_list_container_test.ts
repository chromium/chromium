// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {SettingsFocusListContainerElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

class TestFocusListContainerHostElement extends CrLitElement {
  static get is() {
    return 'test-focus-list-container-host';
  }

  override render() {
    // clang-format off
    return html`
      <button id="outsideButton">Outside</button>
      <settings-focus-list-container id="list"
          .focusSelector="${this.focusSelector}">
        ${this.items.map(item => this.focusSelector ? html`
          <div class="list-item">
            <span>${item}</span>
            <cr-icon-button class="icon-clear"></cr-icon-button>
          </div>
        ` : html`
          <cr-checkbox>${item}</cr-checkbox>
        `)}
      </settings-focus-list-container>
    `;
    // clang-format on
  }

  static override get properties() {
    return {
      focusSelector: {type: String},
      items: {type: Array},
    };
  }

  accessor focusSelector: string = 'cr-icon-button';
  accessor items: string[] = [];
}

customElements.define(
    TestFocusListContainerHostElement.is, TestFocusListContainerHostElement);

suite('SettingsFocusListContainer', function() {
  let host: TestFocusListContainerHostElement;
  let list: SettingsFocusListContainerElement;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    host = document.createElement('test-focus-list-container-host') as
        TestFocusListContainerHostElement;
    host.items = ['item1', 'item2', 'item3'];
    document.body.appendChild(host);
    await microtasksFinished();
    const focusList =
        host.shadowRoot.querySelector('settings-focus-list-container');
    assertTrue(!!focusList);
    list = focusList;
  });

  test('DefaultAndCustomAttributes', function() {
    assertEquals('listbox block', list.getAttribute('focusgroup'));
    assertEquals('listbox', list.getAttribute('role'));

    const customList = document.createElement('settings-focus-list-container');
    customList.setAttribute('focusgroup', 'toolbar block');
    document.body.appendChild(customList);
    assertEquals('toolbar block', customList.getAttribute('focusgroup'));
    assertFalse(customList.hasAttribute('role'));
  });

  test('FocusRestorationOnDelete', async function() {
    let buttons = host.shadowRoot.querySelectorAll('cr-icon-button');
    assertEquals(3, buttons.length);

    // Case 1: Delete last item while focused.
    buttons[2]!.focus();
    assertEquals(buttons[2], host.shadowRoot.activeElement);
    host.items = ['item1', 'item2'];
    await microtasksFinished();

    buttons = host.shadowRoot.querySelectorAll('cr-icon-button');
    assertEquals(2, buttons.length);
    assertEquals(buttons[1], host.shadowRoot.activeElement);

    // Case 2: Delete first item while focused.
    buttons[0]!.focus();
    assertEquals(buttons[0], host.shadowRoot.activeElement);
    host.items = ['item2'];
    await microtasksFinished();

    buttons = host.shadowRoot.querySelectorAll('cr-icon-button');
    assertEquals(1, buttons.length);
    assertEquals(buttons[0], host.shadowRoot.activeElement);

    // Case 3: Delete last remaining item while focused.
    host.items = [];
    await microtasksFinished();
    buttons = host.shadowRoot.querySelectorAll('cr-icon-button');
    assertEquals(0, buttons.length);
    assertNull(host.shadowRoot.activeElement);
  });

  test('FocusRestorationWithoutSelector', async function() {
    host.focusSelector = '';
    host.items = ['item1', 'item2'];
    await microtasksFinished();

    let checkboxes = host.shadowRoot.querySelectorAll('cr-checkbox');
    assertEquals(2, checkboxes.length);

    checkboxes[1]!.focus();
    assertEquals(checkboxes[1], host.shadowRoot.activeElement);
    host.items = ['item1'];
    await microtasksFinished();

    checkboxes = host.shadowRoot.querySelectorAll('cr-checkbox');
    assertEquals(1, checkboxes.length);
    assertEquals(checkboxes[0], host.shadowRoot.activeElement);
  });

  test('DoesNotStealFocusWhenUnfocused', async function() {
    const outsideButton =
        host.shadowRoot.querySelector<HTMLElement>('#outsideButton');
    assertTrue(!!outsideButton);
    const buttons = host.shadowRoot.querySelectorAll('cr-icon-button');

    // Focus last item, then move focus to an outside button before deleting.
    buttons[2]!.focus();
    assertEquals(buttons[2], host.shadowRoot.activeElement);
    outsideButton.focus();
    assertEquals(outsideButton, host.shadowRoot.activeElement);

    host.items = ['item1', 'item2'];
    await microtasksFinished();
    assertEquals(outsideButton, host.shadowRoot.activeElement);
  });
});
