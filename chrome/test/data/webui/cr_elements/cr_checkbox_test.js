// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';

import {keyDownOn, keyUpOn, pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {eventToPromise} from '../test_util.m.js';

// clang-format on

suite('cr-checkbox', function() {
  /** @type {!CrCheckboxElement} */
  let checkbox;

  /** @type {!HTMLElement} */
  let innerCheckbox;

  setup(function() {
    document.body.innerHTML = `
      <cr-checkbox>
        <div>label
          <a>link</a>
        </div>
      </cr-checkbox>
    `;

    checkbox = /** @type {!CrCheckboxElement} */ (
        document.querySelector('cr-checkbox'));
    innerCheckbox = /** @type {!HTMLElement} */ (checkbox.$$('#checkbox'));
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

  /**
   * @param {string} keyName The name of the key to trigger.
   * @param {!HTMLElement=} element
   */
  function triggerKeyPressEvent(keyName, element) {
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

  test('LabelDisplay', function() {
    const labelContainer = checkbox.$['label-container'];
    // Test that there's actually a label that's more than just the padding.
    assertTrue(labelContainer.offsetWidth > 20);

    checkbox.classList.add('no-label');
    assertEquals('none', getComputedStyle(labelContainer).display);
  });

  test('ClickedOnLinkDoesNotToggleCheckbox', function(done) {
    eventToPromise('change', checkbox).then(function() {
      assertFalse(true);
    });

    assertNotChecked();
    const link =
        /** @type {!HTMLAnchorElement} */ (document.querySelector('a'));
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
    document.body.innerHTML = `
      <cr-checkbox id="checkbox" tab-index="-1"></cr-checkbox>
    `;

    checkbox = /** @type {!CrCheckboxElement} */ (
        document.querySelector('cr-checkbox'));
    innerCheckbox = /** @type {!HTMLElement} */ (checkbox.$$('#checkbox'));

    // Should not override tabindex if it is initialized.
    assertEquals(-1, checkbox.tabIndex);
    assertFalse(checkbox.hasAttribute('tabindex'));
    assertEquals('-1', innerCheckbox.getAttribute('tabindex'));
  });

  test('InitializingWithDisabled', function() {
    document.body.innerHTML = `
      <cr-checkbox id="checkbox" disabled></cr-checkbox>
    `;

    checkbox = /** @type {!CrCheckboxElement} */ (
        document.querySelector('cr-checkbox'));
    innerCheckbox = /** @type {!HTMLElement} */ (checkbox.$$('#checkbox'));

    // Initializing with disabled should make tabindex="-1".
    assertEquals(-1, checkbox.tabIndex);
    assertFalse(checkbox.hasAttribute('tabindex'));
    assertEquals('-1', innerCheckbox.getAttribute('tabindex'));
  });

  test('tabindex attribute is controlled by tabIndex', () => {
    document.body.innerHTML = `
      <cr-checkbox id="checkbox" tabindex="-1"></cr-checkbox>
    `;
    checkbox = /** @type {!CrCheckboxElement} */ (
        document.querySelector('cr-checkbox'));
    assertEquals(0, checkbox.tabIndex);
    assertFalse(checkbox.hasAttribute('tabindex'));
    assertEquals('0', innerCheckbox.getAttribute('tabindex'));
  });
});
