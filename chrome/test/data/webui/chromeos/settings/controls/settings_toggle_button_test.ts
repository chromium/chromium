// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://os-settings/os_settings.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {DEFAULT_CHECKED_VALUE, DEFAULT_UNCHECKED_VALUE, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {clearBody} from '../utils.js';
// clang-format on

/** @fileoverview Suite of tests for settings-toggle-button. */
suite('SettingsToggleButton', () => {
  let testElement: SettingsToggleButtonElement;

  // Initialize a checked control before each test.
  setup(() => {
    /**
     * Pref value used in tests, should reflect the 'checked' attribute.
     * Create a new pref for each test() to prevent order (state)
     * dependencies between tests.
     * @type {chrome.settingsPrivate.PrefObject}
     */
    const pref = {
      key: 'test',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    };
    clearBody();
    testElement = document.createElement('settings-toggle-button');
    testElement.set('pref', pref);
    document.body.appendChild(testElement);
  });

  test('value changes on click', () => {
    assertTrue(testElement.checked);
    assertTrue(testElement.pref!.value);

    testElement.click();
    assertFalse(testElement.checked);
    assertFalse(testElement.pref!.value);

    testElement.click();
    assertTrue(testElement.checked);
    assertTrue(testElement.pref!.value);
  });

  test('fires a change event', (done) => {
    testElement.addEventListener('change', () => {
      assertFalse(testElement.checked);
      done();
    });
    assertTrue(testElement.checked);
    testElement.click();
  });

  test('fires a change event for label', (done) => {
    testElement.addEventListener('change', () => {
      assertFalse(testElement.checked);
      done();
    });
    assertTrue(testElement.checked);
    testElement.$.labelWrapper.click();
  });

  test('fires a change event for toggle', (done) => {
    testElement.addEventListener('change', () => {
      assertFalse(testElement.checked);
      done();
    });
    assertTrue(testElement.checked);
    testElement.$.control.click();
  });

  test('fires a single change event per tap', () => {
    let counter = 0;
    testElement.addEventListener('change', () => {
      ++counter;
    });
    testElement.click();
    assertEquals(1, counter);
    testElement.$.labelWrapper.click();
    assertEquals(2, counter);
    testElement.$.control.click();
    assertEquals(3, counter);
  });

  test('does not change when disabled', () => {
    testElement.checked = false;
    testElement.setAttribute('disabled', '');
    assertTrue(testElement.disabled);
    assertTrue(testElement.$.control.disabled);

    testElement.click();
    assertFalse(testElement.checked);
    assertFalse(testElement.$.control.checked);
  });

  test('inverted', () => {
    testElement.inverted = true;
    testElement.set('pref', {
      key: 'test',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    });

    assertTrue(testElement.pref!.value);
    assertFalse(testElement.checked);

    testElement.click();
    assertFalse(testElement.pref!.value);
    assertTrue(testElement.checked);

    testElement.click();
    assertTrue(testElement.pref!.value);
    assertFalse(testElement.checked);
  });

  test('numerical pref', () => {
    const prefNum = {
      key: 'test',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: DEFAULT_CHECKED_VALUE,
    };

    testElement.set('pref', prefNum);
    assertTrue(testElement.checked);

    testElement.click();
    assertFalse(testElement.checked);
    assertEquals(DEFAULT_UNCHECKED_VALUE, prefNum.value);

    testElement.click();
    assertTrue(testElement.checked);
    assertEquals(DEFAULT_CHECKED_VALUE, prefNum.value);
  });

  test('numerical pref with custom values', () => {
    const UNCHECKED_VALUE_1 = 1;
    const UNCHECKED_VALUE_2 = 2;
    const CHECKED_VALUE = 3;

    const prefNum = {
      key: 'test',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: UNCHECKED_VALUE_2,
    };

    testElement.numericUncheckedValues = [UNCHECKED_VALUE_1, UNCHECKED_VALUE_2];
    testElement.numericCheckedValue = CHECKED_VALUE;

    // Test initial 'off' case.
    testElement.set('pref', prefNum);
    assertFalse(testElement.checked);
    assertEquals(UNCHECKED_VALUE_2, prefNum.value);

    // Test 'off' -> 'on' case.
    testElement.click();
    assertTrue(testElement.checked);
    assertEquals(CHECKED_VALUE, prefNum.value);

    // Test 'on' -> 'off' case.
    testElement.click();
    assertFalse(testElement.checked);
    assertEquals(UNCHECKED_VALUE_1, prefNum.value);
  });

  const UNKNOWN_VALUE = 3;

  test('numerical pref with unknown initial value', () => {
    const CUSTOM_UNCHECKED_VALUE = 5;
    const CUSTOM_CHECKED_VALUE = 2;

    const prefNum = {
      key: 'test',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: UNKNOWN_VALUE,
    };

    testElement.numericUncheckedValues = [CUSTOM_UNCHECKED_VALUE];
    testElement.numericCheckedValue = CUSTOM_CHECKED_VALUE;

    testElement.set('pref', prefNum);

    // Unknown value should still count as checked.
    assertTrue(testElement.checked);

    // The control should not clobber an existing unknown value.
    assertEquals(UNKNOWN_VALUE, prefNum.value);

    // Unchecking should still send the unchecked value to prefs.
    testElement.click();
    assertFalse(testElement.checked);
    assertEquals(CUSTOM_UNCHECKED_VALUE, prefNum.value);

    // Checking should still send the normal checked value to prefs.
    testElement.click();
    assertTrue(testElement.checked);
    assertEquals(CUSTOM_CHECKED_VALUE, prefNum.value);
  });

  test('shows controlled indicator when pref is controlled', () => {
    assertFalse(
        !!testElement.shadowRoot!.querySelector('cr-policy-pref-indicator'));

    const pref = {
      key: 'test',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: UNKNOWN_VALUE,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.EXTENSION,
    };

    testElement.set('pref', pref);
    flush();

    assertTrue(
        !!testElement.shadowRoot!.querySelector('cr-policy-pref-indicator'));
  });

  test('no indicator with no-extension-indicator flag', () => {
    assertFalse(
        !!testElement.shadowRoot!.querySelector('cr-policy-pref-indicator'));

    testElement.noExtensionIndicator = true;
    const pref = {
      key: 'test',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: UNKNOWN_VALUE,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.EXTENSION,
    };

    testElement.set('pref', pref);
    flush();

    assertFalse(
        !!testElement.shadowRoot!.querySelector('cr-policy-pref-indicator'));
  });

  test('user control disabled pref', () => {
    const pref = {
      key: 'test',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
      userControlDisabled: true,
    };

    assertFalse(testElement.$.control.disabled);
    testElement.set('pref', pref);
    flush();
    assertTrue(testElement.$.control.disabled);
  });

  test('click on learn more link should not toggle the button', () => {
    let learnMoreLink =
        testElement.shadowRoot!.querySelector<HTMLElement>('#learn-more');
    assertFalse(!!learnMoreLink);
    testElement.set('learnMoreUrl', 'www.google.com');
    flush();

    learnMoreLink =
        testElement.shadowRoot!.querySelector<HTMLElement>('#learn-more');
    assertTrue(!!learnMoreLink);

    assertTrue(testElement.checked);
    flush();

    learnMoreLink!.click();
    assertTrue(testElement.checked);
  });

  test('learn more link should indicate it opens in new tab', () => {
    testElement.set('learnMoreUrl', 'www.google.com');
    flush();
    const learnMoreLink =
        testElement.shadowRoot!.querySelector<HTMLElement>('#learn-more');
    assertTrue(!!learnMoreLink);
    assertEquals(
        learnMoreLink.getAttribute('aria-description'),
        loadTimeData.getString('opensInNewTab'));
  });

  test('set label text should update aria-label of toggle', () => {
    const testLabelText = 'test label text';
    testElement.setAttribute('label', testLabelText);

    const crToggle = testElement.shadowRoot!.querySelector('#control');
    assertTrue(!!crToggle);
    assertEquals(crToggle!.getAttribute('aria-label'), testLabelText);

    const testLabelTextAlt = 'test label text alt';
    testElement.setAttribute('label', testLabelTextAlt);
    assertEquals(crToggle!.getAttribute('aria-label'), testLabelTextAlt);
  });

  test('set aria-label attribute should override aria-label of toggle', () => {
    const testLabelText = 'test label text';
    testElement.setAttribute('label', testLabelText);

    const crToggle = testElement.shadowRoot!.querySelector('#control');
    assertTrue(!!crToggle);
    assertEquals(crToggle!.getAttribute('aria-label'), testLabelText);

    const testAriaLabel = 'test aria label';
    testElement.setAttribute('aria-label', testAriaLabel);
    assertEquals(crToggle!.getAttribute('aria-label'), testAriaLabel);
  });

  test('sub label with action link should have proper role', () => {
    testElement.subLabelWithLink = `<a is="action-link"></a>`;
    flush();

    const subLabelTextWithLink =
        testElement.shadowRoot!.querySelector<HTMLElement>(
            '#sub-label-text-with-link');
    assertTrue(!!subLabelTextWithLink);

    const actionLink = subLabelTextWithLink!.querySelector('a');
    assertTrue(!!actionLink);
    assertEquals(actionLink.getAttribute('role'), 'link');
  });

  test('sub label should be able to have aria-label', () => {
    testElement.subLabelWithLink = `<a aria-label="Label"></a>`;
    flush();

    const subLabelTextWithLink =
        testElement.shadowRoot!.querySelector<HTMLElement>(
            '#sub-label-text-with-link');
    assertTrue(!!subLabelTextWithLink);

    const actionLink = subLabelTextWithLink.querySelector('a');
    assertTrue(!!actionLink);
    assertEquals(actionLink.getAttribute('aria-label'), 'Label');
  });

  test('click on sub label link should not toggle the button', () => {
    let subLabelTextWithLink =
        testElement.shadowRoot!.querySelector('#sub-label-text-with-link');
    assertFalse(!!subLabelTextWithLink);
    testElement.set('subLabelWithLink', `<a href="#"></a>`);
    flush();

    subLabelTextWithLink =
        testElement.shadowRoot!.querySelector('#sub-label-text-with-link');
    assertTrue(!!subLabelTextWithLink);
    const link = subLabelTextWithLink.querySelector('a');
    assertTrue(!!link);

    assertTrue(testElement.checked);
    flush();

    link.click();
    assertTrue(testElement.checked);
  });

  test('click on sub label with link text should toggle the button', () => {
    let subLabelTextWithLink =
        testElement.shadowRoot!.querySelector<HTMLElement>(
            '#sub-label-text-with-link');
    assertFalse(!!subLabelTextWithLink);
    testElement.set('subLabelWithLink', `<a href="#"></a>`);
    flush();

    subLabelTextWithLink = testElement.shadowRoot!.querySelector<HTMLElement>(
        '#sub-label-text-with-link');
    assertTrue(!!subLabelTextWithLink);

    assertTrue(testElement.checked);
    flush();

    subLabelTextWithLink!.click();
    assertFalse(testElement.checked);
  });
});
