// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeHelpContentList, fakePopularHelpContentList} from 'chrome://os-feedback/fake_data.js';
import {HelpContentList, HelpContentType, SearchResult} from 'chrome://os-feedback/feedback_types.js';
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

  /**
   * @param {!HelpContentList} contentList
   * @param {boolean} isQueryEmpty
   * @param {boolean} isPopularContent
   *
   */
  function initializeHelpContentElement(
      contentList, isQueryEmpty, isPopularContent) {
    assertFalse(!!helpContentElement);
    helpContentElement =
        /** @type {!HelpContentElement} */ (
            document.createElement('help-content'));
    assertTrue(!!helpContentElement);

    helpContentElement.searchResult = {
      contentList: contentList,
      isQueryEmpty: isQueryEmpty,
      isPopularContent: isPopularContent
    };

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

  // Verify that all popular help content are displayed.
  function verifyPopularHelpContent() {
    assertEquals(2, getElement('dom-repeat').items.length);
    const helpLinks =
        helpContentElement.shadowRoot.querySelectorAll('.help-item a');
    assertEquals(2, helpLinks.length);

    // Verify the help links are displayed in order with correct title, url and
    // icon.
    assertEquals('fake article', helpLinks[0].innerText);
    assertEquals(
        'https://support.google.com/chromebook/?q=article', helpLinks[0].href);
    verifyIconName(helpLinks[0], fakePopularHelpContentList[0].contentType);

    assertEquals('fake forum', helpLinks[1].innerText);
    assertEquals(
        'https://support.google.com/chromebook/?q=forum', helpLinks[1].href);
    verifyIconName(helpLinks[1], fakePopularHelpContentList[1].contentType);
  }

  /**
   * Test that expected HTML elements are in the element when query is empty.
   */
  test('ColdStart', async () => {
    await initializeHelpContentElement(
        fakePopularHelpContentList, /* isQueryEmpty= */ true,
        /* isPopularContent= */ true);

    // Verify the title is in the helpContentElement.
    const title = getElement('#helpContentLabel');
    assertTrue(!!title);
    assertEquals('Top help content', title.textContent);

    verifyPopularHelpContent();
  });

  /**
   * Test that expected HTML elements are in the element when the query is not
   * empty and there are matches.
   */
  test('SuggestedHelpContentLoaded', async () => {
    await initializeHelpContentElement(
        fakeHelpContentList, /* isQueryEmpty =*/ false,
        /* isPopularContent =*/ false);

    // Verify the title is in the helpContentElement.
    const title = getElement('#helpContentLabel');
    assertTrue(!!title);
    assertEquals('Suggested help content', title.textContent);

    // Verify the help content is populated with correct number of items.
    assertEquals(5, getElement('dom-repeat').items.length);
    const helpLinks =
        helpContentElement.shadowRoot.querySelectorAll('.help-item a');
    assertEquals(5, helpLinks.length);

    // Verify the help links are displayed in order with correct title, url and
    // icon.
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


  /**
   * Test that expected HTML elements are in the element when query is not empty
   * and there are no matches.
   */
  test('NoMatches', async () => {
    await initializeHelpContentElement(
        fakePopularHelpContentList, /* isQueryEmpty= */ false,
        /* isPopularContent= */ true);

    // Verify the title is in the helpContentElement.
    const title = getElement('#helpContentLabel');
    assertTrue(!!title);
    assertEquals(
        'No suggested content. See top help content.', title.textContent);

    verifyPopularHelpContent();
  });
}
