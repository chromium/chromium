// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for managed-footnote. */

// clang-format off
import 'chrome://resources/cr_components/managed_footnote/managed_footnote.js';

import {ManagedFootnoteElement} from 'chrome://resources/cr_components/managed_footnote/managed_footnote.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals,assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

// clang-format on

suite('ManagedFootnoteTest', function() {
  suiteSetup(function() {
    loadTimeData.data = {};
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  /**
   * Resets loadTimeData to the parameters, inserts a <managed-footnote>
   * element in the DOM, and returns it.
   * @param isManaged Whether the footnote should be visible.
   * @param browserMessage String to display inside the element, before href
   *     substitution.
   * @param deviceMessage String to display inside the element, before href
   *     substitution.
   */
  function setupTestElement(
      isManaged: boolean, browserMessage: string, deviceMessage: string,
      managementPageUrl: string): ManagedFootnoteElement {
    loadTimeData.overrideValues({
      chromeManagementUrl: managementPageUrl,
      isManaged: isManaged,
      browserManagedByOrg: browserMessage,
      deviceManagedByOrg: deviceMessage,
    });
    const footnote = document.createElement('managed-footnote');
    document.body.appendChild(footnote);
    flush();
    return footnote;
  }

  test('Hidden When isManaged Is False', function() {
    const footnote = setupTestElement(false, '', '', '');
    assertEquals('none', getComputedStyle(footnote).display);
  });

  test('Reads Attributes From loadTimeData browser message', function() {
    const browserMessage = 'the quick brown fox jumps over the lazy dog';
    const footnote = setupTestElement(true, browserMessage, '', '');

    assertNotEquals('none', getComputedStyle(footnote).display);
    assertTrue(footnote.shadowRoot!.textContent!.includes(browserMessage));
  });

  test('Responds to is-managed-changed events', function() {
    const footnote = setupTestElement(false, '', '', '');
    assertEquals('none', getComputedStyle(footnote).display);

    webUIListenerCallback('is-managed-changed', [true]);
    assertNotEquals('none', getComputedStyle(footnote).display);
  });

  // <if expr="chromeos_ash">
  test('Reads Attributes From loadTimeData device message', function() {
    const browserMessage = 'the quick brown fox jumps over the lazy dog';
    const deviceMessage = 'the lazy dog jumps over the quick brown fox';
    const footnote = setupTestElement(true, browserMessage, deviceMessage, '');

    assertNotEquals('none', getComputedStyle(footnote).display);
    assertTrue(footnote.shadowRoot!.textContent!.includes(browserMessage));

    footnote.showDeviceInfo = true;
    assertTrue(footnote.shadowRoot!.textContent!.includes(deviceMessage));
  });
  // </if>
});
