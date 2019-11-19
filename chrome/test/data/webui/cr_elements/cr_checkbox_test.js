// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
// #import {keyDownOn, keyUpOn, pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
// #import {eventToPromise} from '../test_util.m.js';
// clang-format on

suite('cr-checkbox', function() {
  let checkbox;

  setup(function() {
    PolymerTest.clearBody();
    document.body.innerHTML = `
      <cr-checkbox>
        <div>label
          <a>link</a>
        </div>
      </cr-checkbox>
    `;

    checkbox = document.querySelector('cr-checkbox');
    assertNotChecked();
  });

  function assertChecked() {
    assertTrue(checkbox.checked);
    assertTrue(checkbox.hasAttribute('checked'));
    assertEquals('true', checkbox.$.checkbox.getAttribute('aria-checked'));
  }

  function assertNotChecked() {
    assertFalse(checkbox.checked);
    assertEquals(null, checkbox.getAttribute('checked'));
    assertEquals('false', checkbox.$.checkbox.getAttribute('aria-checked'));
  }

  function assertDisabled() {
    assertTrue(checkbox.disabled);
    assertFalse(checkbox.hasAttribute('tabindex'));
    assertEquals('-1', checkbox.$.checkbox.getAttribute('tabindex'));
    assertTrue(checkbox.hasAttribute('disabled'));
    assertEquals('true', checkbox.$.checkbox.getAttribute('aria-disabled'));
    assertEquals('none', getComputedStyle(checkbox).pointerEvents);
  }

  function assertNotDisabled() {
    assertFalse(checkbox.disabled);
    assertFalse(checkbox.hasAttribute('tabindex'));
    assertEquals('0', checkbox.$.checkbox.getAttribute('tabindex'));
    assertFalse(checkbox.hasAttribute('disabled'));
    assertEquals('false', checkbox.$.checkbox.getAttribute('aria-disabled'));
  }

  /**
   * @param {string} keyName The name of the key to trigger.
   * @param {HTMLElement=} element
   */
  function triggerKeyPressEvent(keyName, element) {
    element = element || checkbox.$.checkbox;
    MockInteractions.pressAndReleaseKeyOn(element, '', undefined, keyName);
  }

  // Test that the control is checked when the user taps on it (no movement
  // between pointerdown and pointerup).
  test('ToggleByMouse', async () => {
    let whenChanged = test_util.eventToPromise('change', checkbox);
    checkbox.click();
    await whenChanged;
    assertChecked();
    whenChanged = test_util.eventToPromise('change', checkbox);
    checkbox.click();
    await whenChanged;
    assertNotChecked();
  });

  // Test that the control is checked when the |checked| attribute is
  // programmatically changed.
  test('ToggleByAttribute', done => {
    test_util.eventToPromise('change', checkbox).then(function() {
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
    let whenChanged = test_util.eventToPromise('change', checkbox);
    checkbox.$.checkbox.click();
    await whenChanged;
    assertChecked();
    whenChanged = test_util.eventToPromise('change', checkbox);
    triggerKeyPressEvent('Enter');
    await whenChanged;
    assertNotChecked();
    whenChanged = test_util.eventToPromise('change', checkbox);
    triggerKeyPressEvent(' ');
    await whenChanged;
    assertChecked();
  });

  // Test that the control is not affected by user interaction when disabled.
  test('ToggleWhenDisabled', function(done) {
    assertNotDisabled();
    checkbox.disabled = true;
    assertDisabled();

    test_util.eventToPromise('change', checkbox).then(function() {
      assertFalse(true);
    });

    checkbox.click();
    assertNotChecked();
    checkbox.$.checkbox.click();
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
    test_util.eventToPromise('change', checkbox).then(function() {
      assertFalse(true);
    });

    assertNotChecked();
    const link = document.querySelector('a');
    link.click();
    assertNotChecked();

    triggerKeyPressEvent('Enter', link);
    assertNotChecked();

    // Wait 1 cycle to make sure change-event was not fired.
    setTimeout(done);
  });

  test('space key down does not toggle', () => {
    assertNotChecked();
    MockInteractions.keyDownOn(checkbox.$.checkbox, null, undefined, ' ');
    assertNotChecked();
  });

  test('space key up toggles', () => {
    assertNotChecked();
    MockInteractions.keyUpOn(checkbox.$.checkbox, null, undefined, ' ');
    assertChecked();
  });

  test('InitializingWithTabindex', function() {
    PolymerTest.clearBody();
    document.body.innerHTML = `
      <cr-checkbox id="checkbox" tab-index="-1"></cr-checkbox>
    `;

    checkbox = document.querySelector('cr-checkbox');

    // Should not override tabindex if it is initialized.
    assertEquals(-1, checkbox.tabIndex);
    assertFalse(checkbox.hasAttribute('tabindex'));
    assertEquals('-1', checkbox.$.checkbox.getAttribute('tabindex'));
  });

  test('InitializingWithDisabled', function() {
    PolymerTest.clearBody();
    document.body.innerHTML = `
      <cr-checkbox id="checkbox" disabled></cr-checkbox>
    `;

    checkbox = document.querySelector('cr-checkbox');

    // Initializing with disabled should make tabindex="-1".
    assertEquals(-1, checkbox.tabIndex);
    assertFalse(checkbox.hasAttribute('tabindex'));
    assertEquals('-1', checkbox.$.checkbox.getAttribute('tabindex'));
  });

  test('tabindex attribute is controlled by tabIndex', () => {
    PolymerTest.clearBody();
    document.body.innerHTML = `
      <cr-checkbox id="checkbox" tabindex="-1"></cr-checkbox>
    `;
    checkbox = document.querySelector('cr-checkbox');
    assertEquals(0, checkbox.tabIndex);
    assertFalse(checkbox.hasAttribute('tabindex'));
    assertEquals('0', checkbox.$.checkbox.getAttribute('tabindex'));
  });
});
