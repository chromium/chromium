// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {CrCheckboxElement} from 'chrome://resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import {keyDownOn, keyUpOn, pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {assertEquals, assertFalse, assertTrue, assertLT, assertGT} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

// clang-format on

suite('cr-checkbox', function() {
  let checkbox: CrCheckboxElement;
  let innerCheckbox: HTMLElement;

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
    pressAndReleaseKeyOn(element || innerCheckbox, 0, undefined, keyName);
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
  test('ToggleByAttribute', done => {
    eventToPromise('change', checkbox).then(function() {
      // Should not fire 'change' event when state is changed programmatically.
      // Only user interaction should result in 'change' event.
      assertFalse(true);
    });

    checkbox.checked = true;
    assertChecked();

    checkbox.checked = false;
    assertNotChecked();

    // Wait 1 cycle to make sure change-event was not fired.
    setTimeout(done);
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
  test('ToggleWhenDisabled', function(done) {
    assertNotDisabled();
    checkbox.disabled = true;
    assertDisabled();

    eventToPromise('change', checkbox).then(function() {
      assertFalse(true);
    });

    checkbox.click();
    assertNotChecked();
    innerCheckbox.click();
    assertNotChecked();
    triggerKeyPressEvent('Enter');
    assertNotChecked();
    triggerKeyPressEvent(' ');
    assertNotChecked();

    // Wait 1 cycle to make sure change-event was not fired.
    setTimeout(done);
  });

  test('LabelDisplay_NoLabel', function() {
    const labelContainer =
        checkbox.shadowRoot!.querySelector<HTMLElement>('#label-container');
    assertTrue(!!labelContainer);

    // Test that there's actually a label that's more than just the padding.
    assertGT(labelContainer.offsetWidth, 20);

    checkbox.classList.add('no-label');
    assertEquals('none', getComputedStyle(labelContainer).display);
  });

  test('LabelDisplay_LabelFirst', () => {
    let checkboxRect = checkbox.$.checkbox.getBoundingClientRect();

    const labelContainer =
        checkbox.shadowRoot!.querySelector<HTMLElement>('#label-container');
    assertTrue(!!labelContainer);
    let labelContainerRect = labelContainer.getBoundingClientRect();

    assertLT(checkboxRect.left, labelContainerRect.left);

    checkbox.classList.add('label-first');
    checkboxRect = checkbox.$.checkbox.getBoundingClientRect();
    labelContainerRect = labelContainer.getBoundingClientRect();
    assertGT(checkboxRect.left, labelContainerRect.left);
  });

  test('ClickedOnLinkDoesNotToggleCheckbox', function(done) {
    eventToPromise('change', checkbox).then(function() {
      assertFalse(true);
    });

    assertNotChecked();
    const link = document.querySelector('a')!;
    link.click();
    assertNotChecked();

    triggerKeyPressEvent('Enter', link);
    assertNotChecked();

    // Wait 1 cycle to make sure change-event was not fired.
    setTimeout(done);
  });

  test('space key down does not toggle', () => {
    assertNotChecked();
    keyDownOn(innerCheckbox, 0, undefined, ' ');
    assertNotChecked();
  });

  test('space key up toggles', () => {
    assertNotChecked();
    keyUpOn(innerCheckbox, 0, undefined, ' ');
    assertChecked();
  });

  test('InitializingWithTabindex', function() {
    document.body.innerHTML = getTrustedHTML`
      <cr-checkbox id="checkbox" tab-index="-1"></cr-checkbox>
    `;

    checkbox = document.querySelector('cr-checkbox')!;
    innerCheckbox =
        checkbox.shadowRoot!.querySelector('#checkbox')! as HTMLElement;

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
    innerCheckbox =
        checkbox.shadowRoot!.querySelector('#checkbox')! as HTMLElement;

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
});
