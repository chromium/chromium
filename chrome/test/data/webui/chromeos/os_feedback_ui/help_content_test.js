// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeHelpContentList} from 'chrome://os-feedback/fake_data.js';
import {HelpContentList, HelpContentType} from 'chrome://os-feedback/feedback_types.js';
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

  /**
   * @param {!Element} linkElement
   * @param {!HelpContentType} expectedContentType
   * */
  function verifyIconName(linkElement, expectedContentType) {
    assertEquals(2, linkElement.children.length);
    // The first child is an iron-icon.
    const iconName = linkElement.children[0].icon;

    if (expectedContentType === HelpContentType.kForum) {
      assertEquals(iconName, 'content-type:forum');
    } else {
      // Both kArticle or kUnknown have the same icon.
      assertEquals(iconName, 'content-type:article');
    }
  }

  /** Test that expected html elements are in the element. */
  test('HelpContentLoaded', async () => {
    await initializeHelpContentElement(fakeHelpContentList);

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
        'https://support.google.com/chromebook/?q=6318213', helpLinks[0].href);
    verifyIconName(helpLinks[0], fakeHelpContentList[0].contentType);

    assertEquals(
        'Why won\'t my wireless mouse with a USB piece wor...?',
        helpLinks[1].innerText);
    assertEquals(
        'https://support.google.com/chromebook/?q=123920509',
        helpLinks[1].href);
    verifyIconName(helpLinks[1], fakeHelpContentList[1].contentType);

    assertEquals('Wifi Issues - only on Chromebooks', helpLinks[2].innerText);
    assertEquals(
        'https://support.google.com/chromebook/?q=114174470',
        helpLinks[2].href);
    verifyIconName(helpLinks[2], fakeHelpContentList[2].contentType);

    assertEquals('Network Connectivity Fault', helpLinks[3].innerText);
    assertEquals(
        'https://support.google.com/chromebook/?q=131459420',
        helpLinks[3].href);
    verifyIconName(helpLinks[3], fakeHelpContentList[3].contentType);

    assertEquals(
        'Connected to WiFi but can\'t connect to the internet',
        helpLinks[4].innerText);
    assertEquals(
        'https://support.google.com/chromebook/?q=22864239', helpLinks[4].href);
    verifyIconName(helpLinks[4], fakeHelpContentList[4].contentType);
  });
}
