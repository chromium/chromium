// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {keyDownOn, keyUpOn, pressAndReleaseKeyOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue, assertLT, assertGT} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('cr-checkbox', function() {
  let checkbox: CrCheckboxElement;
  let innerCheckbox: HTMLElement;

  function waitOneCycle(): Promise<void> {
    return new Promise(res => {
      window.setTimeout(() => res());
    });
  }

  setup(function() {
    document.body.innerHTML = getTrustedHTML`
      <cr-checkbox>
        <div>label
          <a>link</a>
        </div>
      </cr-checkbox>
    `;

    checkbox = document.querySelector('cr-checkbox')!;
    const innerBox =
        checkbox.shadowRoot!.querySelector<HTMLElement>('#checkbox');
    assertTrue(!!innerBox);
    innerCheckbox = innerBox;
    assertNotChecked();
  });

  function assertChecked() {
    assertTrue(checkbox.checked);
    assertTrue(checkbox.hasAttribute('checked'));
    assertEquals('true', innerCheckbox.getAttribute('aria-checked'));
  }

  function assertNotChecked() {
    assertFalse(checkbox.checked);
    assertEquals(null, checkbox.getAttribute('checked'));
    assertEquals('false', innerCheckbox.getAttribute('aria-checked'));
  }

  function assertDisabled() {
    assertTrue(checkbox.disabled);
    assertFalse(checkbox.hasAttribute('tabindex'));
    assertEquals('-1', innerCheckbox.getAttribute('tabindex'));
    assertTrue(checkbox.hasAttribute('disabled'));
    assertEquals('true', innerCheckbox.getAttribute('aria-disabled'));
    assertEquals('none', getComputedStyle(checkbox).pointerEvents);
  }

  function assertNotDisabled() {
    assertFalse(checkbox.disabled);
    assertFalse(checkbox.hasAttribute('tabindex'));
    assertEquals('0', innerCheckbox.getAttribute('tabindex'));
    assertFalse(checkbox.hasAttribute('disabled'));
    assertEquals('false', innerCheckbox.getAttribute('aria-disabled'));
  }

  function triggerKeyPressEvent(keyName: string, element?: HTMLElement) {
    pressAndReleaseKeyOn(element || innerCheckbox, 0, [], keyName);
  }

  // Test that the control is checked when the user taps on it (no movement
  // between pointerdown and pointerup).
  test('ToggleByMouse', async () => {
    let whenChanged = eventToPromise('change', checkbox);
    checkbox.click();
    await whenChanged;
    assertChecked();
    whenChanged = eventToPromise('change', checkbox);
    checkbox.click();
    await whenChanged;
    assertNotChecked();
  });

  // Test that the control is checked when the |checked| attribute is
  // programmatically changed.
  test('ToggleByAttribute', async () => {
    eventToPromise('change', checkbox).then(function() {
      // Should not fire 'change' event when state is changed programmatically.
      // Only user interaction should result in 'change' event.
      assertFalse(true);
    });

    checkbox.checked = true;
    await checkbox.updateComplete;
    assertChecked();

    checkbox.checked = false;
    await checkbox.updateComplete;
    assertNotChecked();

    // Wait 1 cycle to make sure change-event was not fired.
    return waitOneCycle();
  });

  test('Toggle checkbox button click', async () => {
    let whenChanged = eventToPromise('change', checkbox);
    innerCheckbox.click();
    await whenChanged;
    assertChecked();
    whenChanged = eventToPromise('change', checkbox);
    triggerKeyPressEvent('Enter');
    await whenChanged;
    assertNotChecked();
    whenChanged = eventToPromise('change', checkbox);
    triggerKeyPressEvent(' ');
    await whenChanged;
    assertChecked();
  });

  // Test that the control is not affected by user interaction when disabled.
  test('ToggleWhenDisabled', async () => {
    assertNotDisabled();
    checkbox.disabled = true;
    await checkbox.updateComplete;
    assertDisabled();

    eventToPromise('change', checkbox).then(function() {
      assertFalse(true);
    });

    checkbox.click();
    await checkbox.updateComplete;
    assertNotChecked();

    innerCheckbox.click();
    await checkbox.updateComplete;
    assertNotChecked();

    triggerKeyPressEvent('Enter');
    await checkbox.updateComplete;
    assertNotChecked();

    triggerKeyPressEvent(' ');
    await checkbox.updateComplete;
    assertNotChecked();

    // Wait 1 cycle to make sure change-event was not fired.
    return waitOneCycle();
  });

  test('LabelDisplay_NoLabel', function() {
    const labelContainer = checkbox.$.labelContainer;

    // Test that there's actually a label that's more than just the padding.
    assertGT(labelContainer.offsetWidth, 20);

    checkbox.classList.add('no-label');
    assertEquals('none', getComputedStyle(labelContainer).display);
  });

  test('LabelDisplay_LabelFirst', () => {
    let checkboxRect = checkbox.$.checkbox.getBoundingClientRect();

    const labelContainer = checkbox.$.labelContainer;
    let labelContainerRect = labelContainer.getBoundingClientRect();

    assertLT(checkboxRect.left, labelContainerRect.left);

    checkbox.classList.add('label-first');
    checkboxRect = checkbox.$.checkbox.getBoundingClientRect();
    labelContainerRect = labelContainer.getBoundingClientRect();
    assertGT(checkboxRect.left, labelContainerRect.left);
  });

  test('ClickedOnLinkDoesNotToggleCheckbox', async () => {
    eventToPromise('change', checkbox).then(() => {
      assertFalse(true);
    });

    assertNotChecked();
    const link = document.querySelector('a')!;
    link.click();
    await checkbox.updateComplete;
    assertNotChecked();

    triggerKeyPressEvent('Enter', link);
    await checkbox.updateComplete;
    assertNotChecked();

    // Wait 1 cycle to make sure change-event was not fired.
    return waitOneCycle();
  });

  test('space key down does not toggle', async () => {
    assertNotChecked();
    keyDownOn(innerCheckbox, 0, [], ' ');
    await checkbox.updateComplete;
    assertNotChecked();
  });

  test('space key up toggles', async () => {
    assertNotChecked();
    keyUpOn(innerCheckbox, 0, [], ' ');
    await checkbox.updateComplete;
    assertChecked();
  });

  test('InitializingWithTabindex', function() {
    document.body.innerHTML = getTrustedHTML`
      <cr-checkbox id="checkbox" tab-index="-1"></cr-checkbox>
    `;

    checkbox = document.querySelector('cr-checkbox')!;
    innerCheckbox = checkbox.$.checkbox;

    // Should not override tabindex if it is initialized.
    assertEquals(-1, checkbox.tabIndex);
    assertFalse(checkbox.hasAttribute('tabindex'));
    assertEquals('-1', innerCheckbox.getAttribute('tabindex'));
  });

  test('InitializingWithDisabled', function() {
    document.body.innerHTML = getTrustedHTML`
      <cr-checkbox id="checkbox" disabled></cr-checkbox>
    `;

    checkbox = document.querySelector('cr-checkbox')!;
    innerCheckbox = checkbox.$.checkbox;

    // Initializing with disabled should make tabindex="-1".
    assertEquals(-1, checkbox.tabIndex);
    assertFalse(checkbox.hasAttribute('tabindex'));
    assertEquals('-1', innerCheckbox.getAttribute('tabindex'));
  });

  test('tabindex attribute is controlled by tabIndex', () => {
    document.body.innerHTML = getTrustedHTML`
      <cr-checkbox id="checkbox" tabindex="-1"></cr-checkbox>
    `;
    checkbox = document.querySelector('cr-checkbox')!;
    assertEquals(0, checkbox.tabIndex);
    assertFalse(checkbox.hasAttribute('tabindex'));
    assertEquals('0', innerCheckbox.getAttribute('tabindex'));
  });

  // Test that 2-way bindings with Polymer parent elements are updated before
  // the 'change' event is fired.
  test('TwoWayBindingWithPolymerParent', function(done) {
    class TestElement extends PolymerElement {
      static get is() {
        return 'test-element';
      }

      static get template() {
        return html`
          <cr-checkbox checked="{{parentChecked}}"
              on-change="onChange"
              on-checked-changed="onCheckedChanged">
          </cr-checkbox>`;
      }

      static get properties() {
        return {
          parentChecked: Boolean,
        };
      }

      parentChecked: boolean = false;
      private events_: string[] = [];

      onCheckedChanged(e: CustomEvent<{value: boolean}>) {
        assertEquals(this.events_.length === 0 ? false : true, e.detail.value);
        this.events_.push(e.type);
      }

      onChange(e: CustomEvent<boolean>) {
        assertTrue(e.detail);
        assertEquals(e.detail, element.parentChecked);
        this.events_.push(e.type);

        assertDeepEquals(
            ['checked-changed', 'checked-changed', 'change'], this.events_);
        done();
      }
    }

    customElements.define(TestElement.is, TestElement);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const element = document.createElement('test-element') as TestElement;
    document.body.appendChild(element);

    const checkbox = element.shadowRoot!.querySelector('cr-checkbox');
    assertTrue(!!checkbox);
    checkbox.click();
  });
});
