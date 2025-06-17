// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsCheckboxElement} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertFalse, assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

/** @fileoverview Suite of tests for settings-checkbox. */
suite('SettingsCheckbox', function() {
  /**
   * Checkbox created before each test.
   */
  let testElement: SettingsCheckboxElement;

  /**
   * Pref value used in tests, should reflect checkbox 'checked' attribute.
   */
  const pref: chrome.settingsPrivate.PrefObject = {
    key: 'test',
    type: chrome.settingsPrivate.PrefType.BOOLEAN,
    value: true,
  };

  // Initialize a checked settings-checkbox before each test.
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-checkbox');
    testElement.set('pref', pref);
    document.body.appendChild(testElement);
  });

  test('value changes on click', async function() {
    assertTrue(testElement.checked);

    testElement.$.checkbox.click();
    await testElement.$.checkbox.updateComplete;
    assertFalse(testElement.checked);
    assertFalse(pref.value);

    testElement.$.checkbox.click();
    await testElement.$.checkbox.updateComplete;
    assertTrue(testElement.checked);
    assertTrue(pref.value);
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
    assertTrue(testElement.disabled);
    assertTrue(testElement.$.checkbox.disabled);

    testElement.$.checkbox.click();
    await testElement.$.checkbox.updateComplete;
    assertFalse(testElement.checked);
    assertFalse(testElement.$.checkbox.checked);
  });

  test('numerical pref', async function() {
    const prefNum = {
      key: 'test',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 1,
    };

    testElement.set('pref', prefNum);
    assertTrue(testElement.checked);

    testElement.$.checkbox.click();
    await testElement.$.checkbox.updateComplete;
    assertFalse(testElement.checked);
    assertEquals(0, prefNum.value);

    testElement.$.checkbox.click();
    await testElement.$.checkbox.updateComplete;
    assertTrue(testElement.checked);
    assertEquals(1, prefNum.value);
  });

  test('sub label should be able to have an id', () => {
    testElement.subLabelHtml = `<a id="subLabelWithLink"></a>`;
    flush();

    const actionLink =
        testElement.$.subLabel.querySelector('#subLabelWithLink');
    assertTrue(!!actionLink);
  });

  test('sub label should be able to have an aria-label', () => {
    testElement.subLabelHtml = `<a aria-label="Label"></a>`;
    flush();

    const actionLink = testElement.$.subLabel.querySelector('a');
    assertTrue(!!actionLink);
    assertEquals(actionLink.getAttribute('aria-label'), 'Label');
  });

  test(
      'click on sub label link should not toggle the button', async function() {
        testElement.checked = true;
        testElement.subLabelHtml = `<a href="#"></a>`;
        flush();

        const actionLink = testElement.$.subLabel.querySelector('a');
        assertTrue(!!actionLink);

        assertTrue(testElement.checked);

        actionLink.click();
        await testElement.$.checkbox.updateComplete;

        assertTrue(testElement.checked);
      });

  test('click on sub label text should toggle the button', async function() {
    testElement.checked = true;
    testElement.subLabelHtml = `<a href="#"></a>`;
    flush();

    assertTrue(testElement.checked);

    testElement.$.subLabel.click();
    await testElement.$.checkbox.updateComplete;

    assertFalse(testElement.checked);
  });

  test('click on sub label link should fire a custom event', async function() {
    testElement.subLabelHtml = `<a href="#" id="subLabelWithLink"></a>`;
    flush();

    const actionLink = testElement.$.subLabel.querySelector('a');
    assertTrue(!!actionLink);

    const clickEventPromise =
        eventToPromise('sub-label-link-clicked', testElement);
    actionLink.click();
    const clickEvent = await clickEventPromise;
    assertEquals('subLabelWithLink', clickEvent.detail.id);
  });

  test('click on sub label text should not fire a custom event', () => {
    testElement.subLabelHtml = `<a href="#" id="subLabelWithLink"></a>`;
    testElement.addEventListener('sub-label-link-clicked', () => {
      assertNotReached(
          'custom event should not be triggered for non action link clicks.');
    });
    flush();

    testElement.$.subLabel.click();
  });
});
