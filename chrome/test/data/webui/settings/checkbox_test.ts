// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import type {SettingsCheckboxElement} from 'chrome://settings/lazy_load.js';
import {PrefsBrowserProxy, PrefService} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

/** @fileoverview Suite of tests for settings-checkbox. */
suite('SettingsCheckbox', function() {
  /**
   * Checkbox created before each test.
   */
  let testElement: SettingsCheckboxElement;
  let prefsBrowserProxy: TestPrefsBrowserProxy;
  let prefService: PrefService;

  // Initialize a checked settings-checkbox before each test.
  setup(async function() {
    const initialPrefs = [
      {
        key: 'test',
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
    testElement = document.createElement('settings-checkbox');
    testElement.prefKey = 'test';
    document.body.appendChild(testElement);
    await microtasksFinished();
  });

  test('value changes on click', async function() {
    assertTrue(testElement.checked);
    assertTrue(prefService.getPref<boolean>('test').value);

    testElement.$.checkbox.click();
    await microtasksFinished();
    assertFalse(testElement.checked);
    assertFalse(prefService.getPref<boolean>('test').value);

    testElement.$.checkbox.click();
    await microtasksFinished();
    assertTrue(testElement.checked);
    assertTrue(prefService.getPref<boolean>('test').value);
  });

  test('pref change updates value', async function() {
    assertTrue(testElement.checked);

    prefService.setPrefValue('test', false);
    await microtasksFinished();
    assertFalse(testElement.checked);

    prefService.setPrefValue('test', true);
    await microtasksFinished();
    assertTrue(testElement.checked);
  });

  test('fires a change event', function(done) {
    testElement.addEventListener('change', function() {
      assertFalse(testElement.checked);
      done();
    });
    testElement.$.checkbox.click();
  });

  test('does not change when disabled', async function() {
    testElement.checked = false;
    testElement.setAttribute('disabled', '');
    await microtasksFinished();
    assertTrue(testElement.disabled);
    assertTrue(testElement.$.checkbox.disabled);

    testElement.$.checkbox.click();
    await microtasksFinished();
    assertFalse(testElement.checked);
    assertFalse(testElement.$.checkbox.checked);
  });

  test('numerical pref', async function() {
    prefsBrowserProxy.fakeApi.sendPrefChanges([{
      key: 'test',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 1,
    }]);
    await microtasksFinished();
    assertTrue(testElement.checked);

    testElement.$.checkbox.click();
    await microtasksFinished();
    assertFalse(testElement.checked);
    assertEquals(0, prefService.getPref<number>('test').value);

    testElement.$.checkbox.click();
    await microtasksFinished();
    assertTrue(testElement.checked);
    assertEquals(1, prefService.getPref<number>('test').value);
  });

  test('sub label should be able to have an id', async () => {
    testElement.subLabelHtml = '<a id="subLabelWithLink"></a>';
    await microtasksFinished();

    const actionLink =
        testElement.$.subLabel.querySelector('#subLabelWithLink');
    assertTrue(!!actionLink);
  });

  test('sub label should be able to have an aria-label', async () => {
    testElement.subLabelHtml = '<a aria-label="Label"></a>';
    await microtasksFinished();

    const actionLink = testElement.$.subLabel.querySelector('a');
    assertTrue(!!actionLink);
    assertEquals(actionLink.getAttribute('aria-label'), 'Label');
  });

  test(
      'click on sub label link should not toggle the button', async function() {
        testElement.checked = true;
        testElement.subLabelHtml = '<a href="#"></a>';
        await microtasksFinished();

        const actionLink = testElement.$.subLabel.querySelector('a');
        assertTrue(!!actionLink);

        assertTrue(testElement.checked);

        actionLink.click();
        await microtasksFinished();

        assertTrue(testElement.checked);
      });

  test('click on sub label text should toggle the button', async function() {
    testElement.checked = true;
    testElement.subLabelHtml = '<a href="#"></a>';
    await microtasksFinished();

    assertTrue(testElement.checked);

    testElement.$.subLabel.click();
    await microtasksFinished();

    assertFalse(testElement.checked);
  });

  test('click on sub label link should fire a custom event', async function() {
    testElement.subLabelHtml = '<a href="#" id="subLabelWithLink"></a>';
    await microtasksFinished();

    const actionLink = testElement.$.subLabel.querySelector('a');
    assertTrue(!!actionLink);

    const clickEventPromise = eventToPromise<CustomEvent<{id: string}>>(
        'sub-label-link-clicked', testElement);
    actionLink.click();
    const clickEvent = await clickEventPromise;
    assertEquals('subLabelWithLink', clickEvent.detail.id);
  });

  test('click on sub label text should not fire a custom event', async () => {
    testElement.subLabelHtml = '<a href="#" id="subLabelWithLink"></a>';
    testElement.addEventListener('sub-label-link-clicked', () => {
      assertNotReached(
          'custom event should not be triggered for non action link clicks.');
    });
    await microtasksFinished();

    testElement.$.subLabel.click();
  });
});
