// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {DEFAULT_CHECKED_VALUE, DEFAULT_UNCHECKED_VALUE, loadTimeData, PrefService, PrefsBrowserProxy} from 'chrome://settings/settings.js';

import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';
// clang-format on

/** @fileoverview Suite of tests for settings-toggle-button. */
suite('SettingsToggleButton', () => {
  let testElement: SettingsToggleButtonElement;
  let prefsBrowserProxy: TestPrefsBrowserProxy;
  let prefService: PrefService;

  async function createNoToggleOnClickElement():
      Promise<SettingsToggleButtonElement> {
    const noToggleTestElement =
        document.createElement('settings-toggle-button');
    noToggleTestElement.prefKey = 'noToggleOnClickTest';
    noToggleTestElement.noToggleOnHostClick = true;
    document.body.appendChild(noToggleTestElement);
    await microtasksFinished();
    return noToggleTestElement;
  }

  // Initialize a checked control before each test.
  setup(async () => {
    const initialPrefs = [
      {
        key: 'test',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true,
      },
      {
        key: 'noToggleOnClickTest',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true,
      },
    ];
    prefsBrowserProxy = new TestPrefsBrowserProxy(initialPrefs);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);

    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-toggle-button');
    testElement.prefKey = 'test';
    document.body.appendChild(testElement);
  });

  test('value changes on click', () => {
    assertTrue(testElement.checked);
    assertTrue(prefService.getPref<boolean>('test').value);

    testElement.click();
    assertFalse(testElement.checked);
    assertFalse(prefService.getPref<boolean>('test').value);

    testElement.click();
    assertTrue(testElement.checked);
    assertTrue(prefService.getPref<boolean>('test').value);
  });

  test('pref change updates value', async () => {
    assertTrue(testElement.checked);

    prefService.setPrefValue('test', false);
    await microtasksFinished();
    assertFalse(testElement.checked);

    prefService.setPrefValue('test', true);
    await microtasksFinished();
    assertTrue(testElement.checked);
  });

  test('fires a change event', async () => {
    assertTrue(testElement.checked);
    let changeEventPromise =
        eventToPromise<CustomEvent<boolean>>('change', testElement);
    testElement.click();
    let changeEvent = await changeEventPromise;
    assertFalse(changeEvent.detail);

    changeEventPromise =
        eventToPromise<CustomEvent<boolean>>('change', testElement);
    testElement.click();
    changeEvent = await changeEventPromise;
    assertTrue(changeEvent.detail);
  });

  test('fires a change event for label', (done) => {
    testElement.addEventListener('change', () => {
      assertFalse(testElement.checked);
      done();
    });
    assertTrue(testElement.checked);
    testElement.$.labelWrapper.click();
  });

  test('fires a change event for toggle', async () => {
    assertTrue(testElement.checked);
    const toggleChangeEventPromise =
        eventToPromise('change', testElement.$.control);
    const hostChangeEventPromise = eventToPromise('change', testElement);
    testElement.$.control.click();
    const toggleChangeEvent = await toggleChangeEventPromise;
    const hostChangeEvent = await hostChangeEventPromise;
    assertNotEquals(toggleChangeEvent, hostChangeEvent);
    assertEquals(testElement, hostChangeEvent.target);
  });

  test('fires a single change event per tap', async () => {
    let counter = 0;
    testElement.addEventListener('change', () => {
      ++counter;
    });
    let whenFired = eventToPromise('change', testElement);

    testElement.click();
    await whenFired;
    assertEquals(1, counter);

    whenFired = eventToPromise('change', testElement);
    testElement.$.labelWrapper.click();
    await whenFired;
    assertEquals(2, counter);

    whenFired = eventToPromise('change', testElement);
    testElement.$.control.click();
    await whenFired;
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

  test('clicking the host does not toggle pref', async () => {
    const noToggleTestElement = await createNoToggleOnClickElement();
    assertTrue(noToggleTestElement.checked);
    assertTrue(prefService.getPref<boolean>('noToggleOnClickTest').value);
    const changeEventPromise =
        eventToPromise('change', noToggleTestElement).catch(() => {
          // Expect a rejection if 'change' is not fired
          return null;
        });
    // Simulate clicking the host element, not the cr-toggle itself.
    noToggleTestElement.$.labelWrapper.click();
    // Ensure no 'change' event was fired
    const changeEvent = await Promise.race([
      changeEventPromise,
      new Promise(resolve => setTimeout(resolve, 50)),
    ]);  // Small delay to confirm no event
    assertFalse(!!changeEvent, 'Change event should not have fired');
    // Verify the toggle state and pref value remain unchanged
    assertTrue(noToggleTestElement.checked);
    assertTrue(prefService.getPref<boolean>('noToggleOnClickTest').value);
  });

  test('clicking the control still toggles the pref', async () => {
    const noToggleTestElement = await createNoToggleOnClickElement();
    assertTrue(noToggleTestElement.checked);
    assertTrue(prefService.getPref<boolean>('noToggleOnClickTest').value);
    let changeEventPromise =
        eventToPromise<CustomEvent<boolean>>('change', noToggleTestElement);
    // Click specifically on the internal cr-toggle control
    noToggleTestElement.$.control.click();
    // Wait for the change event
    let changeEvent = await changeEventPromise;
    assertFalse(changeEvent.detail);
    // Verify the toggle state and pref value have changed
    assertFalse(noToggleTestElement.checked);
    assertFalse(prefService.getPref<boolean>('noToggleOnClickTest').value);
    // Click again to toggle back
    changeEventPromise =
        eventToPromise<CustomEvent<boolean>>('change', noToggleTestElement);
    noToggleTestElement.$.control.click();
    changeEvent = await changeEventPromise;
    assertTrue(changeEvent.detail);
    assertTrue(noToggleTestElement.checked);
    assertTrue(prefService.getPref<boolean>('noToggleOnClickTest').value);
  });

  test('inverted', async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-toggle-button');
    testElement.inverted = true;
    testElement.prefKey = 'test';
    document.body.appendChild(testElement);
    await microtasksFinished();

    assertTrue(prefService.getPref<boolean>('test').value);
    assertFalse(testElement.checked);

    testElement.click();
    assertFalse(prefService.getPref<boolean>('test').value);
    assertTrue(testElement.checked);

    testElement.click();
    assertTrue(prefService.getPref<boolean>('test').value);
    assertFalse(testElement.checked);
  });

  test('numerical pref', () => {
    prefsBrowserProxy.fakeApi.sendPrefChanges([{
      key: 'test',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: DEFAULT_CHECKED_VALUE,
    }]);
    assertTrue(testElement.checked);

    testElement.click();
    assertFalse(testElement.checked);
    assertEquals(
        DEFAULT_UNCHECKED_VALUE, prefService.getPref<number>('test').value);

    testElement.click();
    assertTrue(testElement.checked);
    assertEquals(
        DEFAULT_CHECKED_VALUE, prefService.getPref<number>('test').value);
  });

  test('numerical pref with custom values', () => {
    const UNCHECKED_VALUE_1 = 1;
    const UNCHECKED_VALUE_2 = 2;
    const CHECKED_VALUE = 3;

    testElement.numericUncheckedValues = [UNCHECKED_VALUE_1, UNCHECKED_VALUE_2];
    testElement.numericCheckedValue = CHECKED_VALUE;

    // Test initial 'off' case.
    prefsBrowserProxy.fakeApi.sendPrefChanges([{
      key: 'test',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: UNCHECKED_VALUE_2,
    }]);
    assertFalse(testElement.checked);
    assertEquals(UNCHECKED_VALUE_2, prefService.getPref<number>('test').value);

    // Test 'off' -> 'on' case.
    testElement.click();
    assertTrue(testElement.checked);
    assertEquals(CHECKED_VALUE, prefService.getPref<number>('test').value);

    // Test 'on' -> 'off' case.
    testElement.click();
    assertFalse(testElement.checked);
    assertEquals(UNCHECKED_VALUE_1, prefService.getPref<number>('test').value);
  });

  const UNKNOWN_VALUE = 3;

  test('numerical pref with unknown initial value', () => {
    const CUSTOM_UNCHECKED_VALUE = 5;
    const CUSTOM_CHECKED_VALUE = 2;

    testElement.numericUncheckedValues = [CUSTOM_UNCHECKED_VALUE];
    testElement.numericCheckedValue = CUSTOM_CHECKED_VALUE;

    prefsBrowserProxy.fakeApi.sendPrefChanges([{
      key: 'test',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: UNKNOWN_VALUE,
    }]);

    // Unknown value should still count as checked.
    assertTrue(testElement.checked);

    // The control should not clobber an existing unknown value.
    assertEquals(UNKNOWN_VALUE, prefService.getPref<number>('test').value);

    // Unchecking should still send the unchecked value to prefs.
    testElement.click();
    assertFalse(testElement.checked);
    assertEquals(
        CUSTOM_UNCHECKED_VALUE, prefService.getPref<number>('test').value);

    // Checking should still send the normal checked value to prefs.
    testElement.click();
    assertTrue(testElement.checked);
    assertEquals(
        CUSTOM_CHECKED_VALUE, prefService.getPref<number>('test').value);
  });

  test('shows controlled indicator when pref is controlled', () => {
    assertFalse(
        !!testElement.shadowRoot!.querySelector('cr-policy-pref-indicator'));

    prefsBrowserProxy.fakeApi.sendPrefChanges([{
      key: 'test',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: UNKNOWN_VALUE,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.EXTENSION,
    }]);
    flush();

    assertTrue(
        !!testElement.shadowRoot!.querySelector('cr-policy-pref-indicator'));
  });

  test('no indicator with no-extension-indicator flag', () => {
    assertFalse(
        !!testElement.shadowRoot!.querySelector('cr-policy-pref-indicator'));

    testElement.noExtensionIndicator = true;
    prefsBrowserProxy.fakeApi.sendPrefChanges([{
      key: 'test',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: UNKNOWN_VALUE,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.EXTENSION,
    }]);
    flush();

    assertFalse(
        !!testElement.shadowRoot!.querySelector('cr-policy-pref-indicator'));
  });

  test('user control disabled pref', () => {
    assertFalse(testElement.$.control.disabled);
    prefsBrowserProxy.fakeApi.sendPrefChanges([{
      key: 'test',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
      userControlDisabled: true,
    }]);
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

    learnMoreLink.click();
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
    assertEquals(crToggle.getAttribute('aria-label'), testLabelText);

    const testLabelTextAlt = 'test label text alt';
    testElement.setAttribute('label', testLabelTextAlt);
    assertEquals(crToggle.getAttribute('aria-label'), testLabelTextAlt);
  });

  test('set aria-label attribute should override aria-label of toggle', () => {
    const testLabelText = 'test label text';
    testElement.setAttribute('label', testLabelText);

    const crToggle = testElement.shadowRoot!.querySelector('#control');
    assertTrue(!!crToggle);
    assertEquals(crToggle.getAttribute('aria-label'), testLabelText);

    const testAriaLabel = 'test aria label';
    testElement.setAttribute('aria-label', testAriaLabel);
    assertEquals(crToggle.getAttribute('aria-label'), testAriaLabel);
  });

  test('sub label with action link should have proper role', () => {
    testElement.subLabelWithLink = '<a is="action-link"></a>';
    flush();

    const subLabelTextWithLink =
        testElement.shadowRoot!.querySelector<HTMLElement>(
            '#sub-label-text-with-link');
    assertTrue(!!subLabelTextWithLink);

    const actionLink = subLabelTextWithLink.querySelector('a');
    assertTrue(!!actionLink);
    assertEquals(actionLink.getAttribute('role'), 'link');
  });

  test('sub label should be able to have aria-label', () => {
    testElement.subLabelWithLink = '<a aria-label="Label"></a>';
    flush();

    const subLabelTextWithLink =
        testElement.shadowRoot!.querySelector<HTMLElement>(
            '#sub-label-text-with-link');
    assertTrue(!!subLabelTextWithLink);

    const actionLink = subLabelTextWithLink.querySelector('a');
    assertTrue(!!actionLink);
    assertEquals(actionLink.getAttribute('aria-label'), 'Label');
  });

  test('shows more-actions-after slot content', () => {
    const slottedContent = document.createElement('div');
    slottedContent.setAttribute('slot', 'more-actions-after');
    slottedContent.textContent = 'Slotted content';
    testElement.appendChild(slottedContent);
    flush();

    const slot = testElement.shadowRoot!.querySelector<HTMLSlotElement>(
        'slot[name="more-actions-after"]');
    assertTrue(!!slot);
    const assignedNodes = slot.assignedNodes({flatten: true});
    assertEquals(1, assignedNodes.length);
    assertEquals(slottedContent, assignedNodes[0]);
    assertEquals('Slotted content', assignedNodes[0]!.textContent!.trim());
  });

  // <if expr="is_chromeos">
  test('click on sub label link should not toggle the button', () => {
    let subLabelTextWithLink =
        testElement.shadowRoot!.querySelector('#sub-label-text-with-link');
    assertFalse(!!subLabelTextWithLink);
    testElement.set('subLabelWithLink', '<a href="#"></a>');
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
    testElement.set('subLabelWithLink', '<a href="#"></a>');
    flush();

    subLabelTextWithLink = testElement.shadowRoot!.querySelector<HTMLElement>(
        '#sub-label-text-with-link');
    assertTrue(!!subLabelTextWithLink);

    assertTrue(testElement.checked);
    flush();

    subLabelTextWithLink.click();
    assertFalse(testElement.checked);
  });
  // </if>
});
