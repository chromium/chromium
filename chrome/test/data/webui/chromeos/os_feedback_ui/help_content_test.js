// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeHelpContentList, fakePopularHelpContentList} from 'chrome://os-feedback/fake_data.js';
import {HelpContentList, HelpContentType, SearchResult} from 'chrome://os-feedback/feedback_types.js';
import {HelpContentElement} from 'chrome://os-feedback/help_content.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {isVisible} from '../test_util.js';

export function helpContentTestSuite() {
  /** @type {?HelpContentElement} */
  let helpContentElement = null;

  const noContentImgSelector = 'img[alt="Help content isn\'t available"]';
  const offlineImgSelector = 'img[alt="Device is offline"]';

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
      isPopularContent: isPopularContent,
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
    assertEquals(1, linkElement.children.length);
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
    assertEquals('fake article', helpLinks[0].innerText.trim());
    assertEquals(
        'https://support.google.com/chromebook/?q=article', helpLinks[0].href);
    verifyIconName(helpLinks[0], fakePopularHelpContentList[0].contentType);

    assertEquals('fake forum', helpLinks[1].innerText.trim());
    assertEquals(
        'https://support.google.com/chromebook/?q=forum', helpLinks[1].href);
    verifyIconName(helpLinks[1], fakePopularHelpContentList[1].contentType);
  }

  function goOffline() {
    // Simulate going offline.
    window.dispatchEvent(new CustomEvent('offline'));
    return flushTasks();
  }

  function goOnline() {
    // Simulate going online.
    window.dispatchEvent(new CustomEvent('online'));
    return flushTasks();
  }

  /**
   * Test that expected HTML elements are in the element when query is empty.
   */
  test('ColdStart', async () => {
    await initializeHelpContentElement(
        fakePopularHelpContentList, /* isQueryEmpty= */ true,
        /* isPopularContent= */ true);

    // Verify the title is in the helpContentElement.
    const title = getElement('.help-content-label');
    assertTrue(!!title);
    assertEquals('Top help content', title.textContent);

    // Verify the help content Icon is in the page.
    const helpContentIcon = getElement('#helpContentIcon');
    assertTrue(!!helpContentIcon);
    // The help content icon is not visible.
    assertFalse(isVisible(helpContentIcon));

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
    const title = getElement('.help-content-label');
    assertTrue(!!title);
    assertEquals('Suggested help content', title.textContent);

    // The help content icon is visible.
    const helpContentIcon = getElement('#helpContentIcon');
    assertTrue(isVisible(helpContentIcon));

    // Verify the help content is populated with correct number of items.
    assertEquals(5, getElement('dom-repeat').items.length);
    const helpLinks =
        helpContentElement.shadowRoot.querySelectorAll('.help-item a');
    assertEquals(5, helpLinks.length);

    // Verify the help links are displayed in order with correct title, url and
    // icon.
    assertEquals('Fix connection problems', helpLinks[0].innerText.trim());
    assertEquals(
        'https://support.google.com/chromebook/?q=6318213', helpLinks[0].href);
    verifyIconName(helpLinks[0], fakeHelpContentList[0].contentType);

    assertEquals(
        'Why won\'t my wireless mouse with a USB piece wor...?',
        helpLinks[1].innerText.trim());
    assertEquals(
        'https://support.google.com/chromebook/?q=123920509',
        helpLinks[1].href);
    verifyIconName(helpLinks[1], fakeHelpContentList[1].contentType);

    assertEquals(
        'Wifi Issues - only on Chromebooks', helpLinks[2].innerText.trim());
    assertEquals(
        'https://support.google.com/chromebook/?q=114174470',
        helpLinks[2].href);
    verifyIconName(helpLinks[2], fakeHelpContentList[2].contentType);

    assertEquals('Network Connectivity Fault', helpLinks[3].innerText.trim());
    assertEquals(
        'https://support.google.com/chromebook/?q=131459420',
        helpLinks[3].href);
    verifyIconName(helpLinks[3], fakeHelpContentList[3].contentType);

    assertEquals(
        'Connected to WiFi but can\'t connect to the internet',
        helpLinks[4].innerText.trim());
    assertEquals(
        'https://support.google.com/chromebook/?q=22864239', helpLinks[4].href);
    verifyIconName(helpLinks[4], fakeHelpContentList[4].contentType);

    // Content not available image should be invisible.
    assertFalse(isVisible(getElement(noContentImgSelector)));
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
    const title = getElement('.help-content-label');
    assertTrue(!!title);
    assertEquals(
        'No suggested content. See top help content.', title.textContent);

    // The help content icon is not visible.
    assertFalse(isVisible(getElement('#helpContentIcon')));

    verifyPopularHelpContent();
    // Content not available image should be invisible.
    assertFalse(isVisible(getElement(noContentImgSelector)));
  });

  /**
   * Test that the offline-only elements render when offline, and that the
   * online-only elements render when online.
   */
  test('OfflineMessage', async () => {
    await initializeHelpContentElement(
        fakePopularHelpContentList, /* isQueryEmpty= */ true,
        /* isPopularContent= */ true);

    await goOffline();

    // Offline-only content should exist in the DOM when offline.
    assertTrue(isVisible(getElement(offlineImgSelector)));
    // Content not available image should be invisible.
    assertFalse(isVisible(getElement(noContentImgSelector)));

    // Online-only content should *not* exist in the DOM when offline.
    assertFalse(isVisible(getElement('.help-item-icon')));

    await goOnline();

    // Offline-only content should *not* exist in the DOM when online.
    assertFalse(isVisible(getElement('offlineImgSelector')));

    // Online-only content should exist in the DOM when online.
    assertTrue(isVisible(getElement('.help-item-icon')));
    // Content not available image should be invisible.
    assertFalse(isVisible(getElement(noContentImgSelector)));
  });

  /**
   * Test that the help content title shows the correct text when the query
   * doesn't match and the device goes offline.
   */
  test('OfflineTitleWhenNoMatches', async () => {
    // Initialize element with no query matches.
    await initializeHelpContentElement(
        fakePopularHelpContentList, /* isQueryEmpty= */ false,
        /* isPopularContent= */ true);

    // Verify the title is what we expect when there are no matches.
    let title = getElement('.help-content-label');
    assertTrue(!!title);
    assertEquals(
        'No suggested content. See top help content.', title.textContent);

    // Content not available image should be invisible.
    assertFalse(isVisible(getElement(noContentImgSelector)));
    await goOffline();

    title = getElement('.help-content-label');
    // When offline, we expect the title to always be "Top help content".
    assertTrue(!!title);
    assertEquals('Top help content', title.textContent);
    // Content not available image should be invisible.
    assertFalse(isVisible(getElement(noContentImgSelector)));
  });

  /**
   * Test that the help content title shows the correct text when we previously
   * were displaying suggested help content and the device goes offline.
   */
  test('OfflineTitleWhenSuggestedContentExists', async () => {
    // Initialize element with no query matches.
    await initializeHelpContentElement(
        fakePopularHelpContentList, /* isQueryEmpty= */ false,
        /* isPopularContent= */ false);

    // Verify the title is what we expect when there are suggested matches.
    let title = getElement('.help-content-label');
    assertTrue(!!title);
    assertEquals('Suggested help content', title.textContent);

    // Content not available image should be invisible.
    assertFalse(isVisible(getElement(noContentImgSelector)));

    await goOffline();

    title = getElement('.help-content-label');
    // When offline, we expect the title to always be "Top help content".
    assertTrue(!!title);
    assertEquals('Top help content', title.textContent);

    // Content not available image should be invisible.
    assertFalse(isVisible(getElement(noContentImgSelector)));
  });

  /**
   * Test that when help content isn't available, the correct image is
   * displayed.
   *
   * Case 1: the query is empty.
   */
  test('TopHelpContentNotAvailable', async () => {
    // Initialize element with no content and empty query.
    await initializeHelpContentElement(
        /* contentList= */[], /* isQueryEmpty= */ true,
        /* isPopularContent= */ true);

    // Verify the title is what we expect when showing top content.
    const title = getElement('.help-content-label');
    assertTrue(!!title);
    assertEquals('Top help content', title.textContent);

    // Content not available image should be visible.
    assertTrue(isVisible(getElement(noContentImgSelector)));
    assertFalse(isVisible(getElement(offlineImgSelector)));

    await goOffline();

    // When offline, should show offline message.
    assertTrue(isVisible(getElement(offlineImgSelector)));
    // Content not available image should be invisible.
    assertFalse(isVisible(getElement(noContentImgSelector)));
  });

  /**
   * Test that when help content isn't available, the correct image is
   * displayed.
   *
   * Case 2: the query is NOT empty.
   */
  test('SuggestedHelpContentNotAvailable', async () => {
    // Initialize element with no content and empty query.
    await initializeHelpContentElement(
        /* contentList= */[], /* isQueryEmpty= */ false,
        /* isPopularContent= */ false);

    // Verify the title is what we expect when there may be suggested matches.
    const title = getElement('.help-content-label');
    assertTrue(!!title);
    assertEquals('Suggested help content', title.textContent);

    // Content not available image should be visible.
    assertTrue(isVisible(getElement(noContentImgSelector)));
    assertFalse(isVisible(getElement(offlineImgSelector)));

    await goOffline();

    // When offline, should show offline message.
    assertTrue(isVisible(getElement(offlineImgSelector)));
    // Content not available image should be invisible.
    assertFalse(isVisible(getElement(noContentImgSelector)));
  });
}
