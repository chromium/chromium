// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeHelpContentList} from 'chrome://os-feedback/fake_data.js';
import {HelpContentList} from 'chrome://os-feedback/feedback_types.js';
import {HelpContentElement} from 'chrome://os-feedback/help_content.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

export function helpContentTestSuite() {
  /** @type {?HelpContentElement} */
  let helpContentElement = null;

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    helpContentElement.remove();
    helpContentElement = null;
  });

  /** @param {!HelpContentList} contentList */
  function initializeHelpContentElement(contentList) {
    assertFalse(!!helpContentElement);
    helpContentElement =
        /** @type {!HelpContentElement} */ (
            document.createElement('help-content'));
    assertTrue(!!helpContentElement);

    helpContentElement.helpContentList = contentList;
    document.body.appendChild(helpContentElement);

    return flushTasks();
  }

  /**
   * Helper function that returns the first Element within the element that
   * matches the specified selector.
   * @param {string} selector
   * */
  function getElement(selector) {
    return helpContentElement.shadowRoot.querySelector(selector);
  }

  /** Test that expected html elements are in the element. */
  test('HelpContentLoaded', () => {
    return initializeHelpContentElement(fakeHelpContentList).then(() => {
      // Verify the title is in the helpContentElement.
      const title = getElement('#helpContentLabel');
      assertTrue(!!title);
      assertEquals('Suggested help content:', title.textContent);

      // Verify the help content is populated with correct number of items.
      assertEquals(5, getElement('dom-repeat').items.length);
      const helpLinks =
          helpContentElement.shadowRoot.querySelectorAll('.help-item a');
      assertEquals(5, helpLinks.length);

      // Verify the help links are displayed in order with correct title and
      // url.
      assertEquals('Fix connection problems', helpLinks[0].innerText);
      assertEquals(
          'https://support.google.com/chromebook/?q=6318213',
          helpLinks[0].href);

      assertEquals(
          'Why won\'t my wireless mouse with a USB piece wor...?',
          helpLinks[1].innerText);
      assertEquals(
          'https://support.google.com/chromebook/?q=123920509',
          helpLinks[1].href);

      assertEquals('Wifi Issues - only on Chromebooks', helpLinks[2].innerText);
      assertEquals(
          'https://support.google.com/chromebook/?q=114174470',
          helpLinks[2].href);

      assertEquals('Network Connectivity Fault', helpLinks[3].innerText);
      assertEquals(
          'https://support.google.com/chromebook/?q=131459420',
          helpLinks[3].href);

      assertEquals(
          'Connected to WiFi but can\'t connect to the internet',
          helpLinks[4].innerText);
      assertEquals(
          'https://support.google.com/chromebook/?q=22864239',
          helpLinks[4].href);
    });
  });
}