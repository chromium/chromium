// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-feedback/help_content.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {IronIconElement} from '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import {fakeHelpContentList, fakePopularHelpContentList} from 'chrome://os-feedback/fake_data.js';
import {HelpContentList} from 'chrome://os-feedback/feedback_types.js';
import {HelpContentElement} from 'chrome://os-feedback/help_content.js';
import {HelpContentType} from 'chrome://os-feedback/os_feedback_ui.mojom-webui.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {DomRepeat} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('helpContentTestSuite', () => {
  let helpContentElement: HelpContentElement;

  const noContentImgSelector = 'img[alt="Help content isn\'t available"]';
  const noContentSvgSelector = '#noContentSvg';
  const offlineImgSelector = 'img[alt="Device is offline"]';
  const offlineSvgSelector = '#offlineSvg';

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });


  function getElement(selector: string): HTMLElement|null {
    return helpContentElement.shadowRoot!.querySelector(selector);
  }

  function initializeHelpContentElement(
      contentList: HelpContentList, isQueryEmpty: boolean,
      isPopularContent: boolean) {
    helpContentElement = document.createElement('help-content');
    assert(helpContentElement);

    helpContentElement.searchResult = {
      contentList: contentList,
      isQueryEmpty: isQueryEmpty,
      isPopularContent: isPopularContent,
    };

    document.body.appendChild(helpContentElement);

    return flushTasks();
  }

  function verifyIconName(
      linkElement: HTMLAnchorElement, expectedContentType: HelpContentType) {
    assertEquals(1, linkElement.children.length);
    // The first child is an iron-icon.
    const iconName = (linkElement.children[0] as IronIconElement).icon;

    if (expectedContentType === HelpContentType.kForum) {
      assertEquals(iconName, 'content-type:forum');
    } else {
      // Both kArticle or kUnknown have the same icon.
      assertEquals(iconName, 'content-type:article');
    }
  }

  // Verify that all popular help content are displayed.
  function verifyPopularHelpContent() {
    assertEquals(
        2,
        strictQuery('dom-repeat', helpContentElement.shadowRoot, DomRepeat)
            .items!.length);
    const helpLinks =
        helpContentElement.shadowRoot!.querySelectorAll('.help-item a');
    assertEquals(2, helpLinks!.length);

    // Verify the help links are displayed in order with correct title, url
    // and icon.
    const link1 = helpLinks[0] as HTMLAnchorElement;
    assertEquals('fake article', link1.innerText.trim());
    assertEquals(
        'https://support.google.com/chromebook/?q=article', link1.href);
    verifyIconName(link1, fakePopularHelpContentList![0]!.contentType);

    const link2 = helpLinks[1] as HTMLAnchorElement;
    assertEquals('fake forum', link2.innerText.trim());
    assertEquals('https://support.google.com/chromebook/?q=forum', link2.href);
    verifyIconName(link2, fakePopularHelpContentList![1]!.contentType);
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
   * Test that expected HTML elements are in the element when query is
   * empty.
   */
  test('ColdStart', async () => {
    await initializeHelpContentElement(
        fakePopularHelpContentList, /* isQueryEmpty= */ true,
        /* isPopularContent= */ true);

    // Verify the title is in the helpContentElement.
    const title = strictQuery(
        '.help-content-label', helpContentElement.shadowRoot, HTMLElement);
    assertTrue(!!title);
    assertEquals('Top help content', title.textContent);

    // Verify the help content Icon is in the page.
    const helpContentIcon = strictQuery(
        '#helpContentIcon', helpContentElement.shadowRoot, HTMLElement);
    assertTrue(!!helpContentIcon);
    // The help content icon is not visible.
    assertFalse(isVisible(helpContentIcon));

    verifyPopularHelpContent();
  });

  /**
   * Test that expected HTML elements are in the element when the query is
   * not empty and there are matches.
   */
  test('SuggestedHelpContentLoaded', async () => {
    await initializeHelpContentElement(
        fakeHelpContentList, /* isQueryEmpty =*/ false,
        /* isPopularContent =*/ false);

    // Verify the title is in the helpContentElement.
    const title = strictQuery(
        '.help-content-label', helpContentElement.shadowRoot, HTMLElement);
    assertTrue(!!title);
    assertEquals('Suggested help content', title.textContent);

    // The help content icon is visible.
    const helpContentIcon = strictQuery(
        '#helpContentIcon', helpContentElement.shadowRoot, HTMLElement);
    assertTrue(isVisible(helpContentIcon));

    // Verify the help content is populated with correct number of items.
    assertEquals(
        5,
        strictQuery('dom-repeat', helpContentElement.shadowRoot, DomRepeat)
            .items!.length);
    const helpLinks =
        helpContentElement.shadowRoot!.querySelectorAll('.help-item a');
    assertEquals(5, helpLinks.length);

    // Verify the help links are displayed in order with correct title, url
    // and icon.
    const link1 = helpLinks[0] as HTMLAnchorElement;
    const link2 = helpLinks[1] as HTMLAnchorElement;
    const link3 = helpLinks[2] as HTMLAnchorElement;
    const link4 = helpLinks[3] as HTMLAnchorElement;
    const link5 = helpLinks[4] as HTMLAnchorElement;
    assertEquals('Fix connection problems', link1.innerText.trim());
    assertEquals(
        'https://support.google.com/chromebook/?q=6318213', link1.href);
    verifyIconName(link1, fakeHelpContentList[0]!.contentType);

    assertEquals(
        'Why won\'t my wireless mouse with a USB piece wor...?',
        link2.innerText.trim());
    assertEquals(
        'https://support.google.com/chromebook/?q=123920509', link2.href);
    verifyIconName(link2, fakeHelpContentList[1]!.contentType);

    assertEquals('Wifi Issues - only on Chromebooks', link3.innerText.trim());
    assertEquals(
        'https://support.google.com/chromebook/?q=114174470', link3.href);
    verifyIconName(link3, fakeHelpContentList[2]!.contentType);

    assertEquals('Network Connectivity Fault', link4.innerText.trim());
    assertEquals(
        'https://support.google.com/chromebook/?q=131459420', link4.href);
    verifyIconName(link4, fakeHelpContentList[3]!.contentType);

    assertEquals(
        'Connected to WiFi but can\'t connect to the internet',
        link5.innerText.trim());
    assertEquals(
        'https://support.google.com/chromebook/?q=22864239', link5.href);
    verifyIconName(link5, fakeHelpContentList[4]!.contentType);

    // Content not available image should be invisible.
    assertFalse(isVisible(getElement(noContentImgSelector)));
  });


  /**
   * Test that expected HTML elements are in the element when query is not
   * empty and there are no matches.
   */
  test('NoMatches', async () => {
    await initializeHelpContentElement(
        fakePopularHelpContentList, /* isQueryEmpty= */ false,
        /* isPopularContent= */ true);

    // Verify the title is in the helpContentElement.
    const title = strictQuery(
        '.help-content-label', helpContentElement.shadowRoot, HTMLElement);
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
    assertTrue(isVisible(getElement(offlineSvgSelector)));
    // Content not available image should be invisible.
    assertFalse(isVisible(getElement(noContentSvgSelector)));

    // Online-only content should *not* exist in the DOM when offline.
    assertFalse(isVisible(getElement('.help-item-icon')));

    await goOnline();

    // Offline-only content should *not* exist in the DOM when online.
    assertFalse(isVisible(getElement('offlineImgSelector')));

    // Online-only content should exist in the DOM when online.
    assertTrue(isVisible(getElement('.help-item-icon')));
    // Content not available image should be invisible.
    assertFalse(isVisible(getElement(noContentSvgSelector)));
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
    let title = strictQuery(
        '.help-content-label', helpContentElement.shadowRoot, HTMLElement);
    assertTrue(!!title);
    assertEquals(
        'No suggested content. See top help content.', title.textContent);

    // Content not available image should be invisible.
    assertFalse(isVisible(getElement(
        noContentImgSelector,
        )));
    await goOffline();

    title = strictQuery(
        '.help-content-label', helpContentElement.shadowRoot, HTMLElement);
    // When offline, we expect the title to always be "Top help content".
    assertTrue(!!title);
    assertEquals('Top help content', title.textContent);
    // Content not available image should be invisible.
    assertFalse(isVisible(getElement(noContentImgSelector)));
  });

  /**
   * Test that the help content title shows the correct text when we
   * previously were displaying suggested help content and the device goes
   * offline.
   */
  test('OfflineTitleWhenSuggestedContentExists', async () => {
    // Initialize element with no query matches.
    await initializeHelpContentElement(
        fakePopularHelpContentList, /* isQueryEmpty= */ false,
        /* isPopularContent= */ false);

    // Verify the title is what we expect when there are suggested matches.
    let title = strictQuery(
        '.help-content-label', helpContentElement.shadowRoot, HTMLElement);
    assertTrue(!!title);
    assertEquals('Suggested help content', title.textContent);

    // Content not available image should be invisible.
    assertFalse(isVisible(getElement(noContentImgSelector)));

    await goOffline();

    title = strictQuery(
        '.help-content-label', helpContentElement.shadowRoot, HTMLElement);
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
    const title = strictQuery(
        '.help-content-label', helpContentElement.shadowRoot, HTMLElement);
    assertTrue(!!title);
    assertEquals('Top help content', title.textContent);

    // Content not available image should be visible.
    assertTrue(isVisible(getElement(noContentSvgSelector)));
    assertFalse(isVisible(getElement(offlineSvgSelector)));

    await goOffline();

    // When offline, should show offline message.
    assertTrue(isVisible(getElement(offlineSvgSelector)));
    // Content not available image should be invisible.
    assertFalse(isVisible(getElement(noContentSvgSelector)));
  });

  /**
   * Test that when help content isn't available, the correct image is
   * displayed.
   *
   * Case 2: the query is NOT empty.
   */
  // TODO(crbug.com/40884343): Flaky.
  test.skip('SuggestedHelpContentNotAvailable', async () => {
    // Initialize element with no content and empty query.
    await initializeHelpContentElement(
        /* contentList= */[], /* isQueryEmpty= */ false,
        /* isPopularContent= */ false);

    // Verify the title is what we expect when there may be suggested
    // matches.
    const title = strictQuery(
        '.help-content-label', helpContentElement.shadowRoot, HTMLElement);
    assertTrue(!!title);
    assertEquals('Suggested help content', title.textContent);

    // Content not available image should be visible.
    assertTrue(isVisible(strictQuery(
        noContentImgSelector, helpContentElement.shadowRoot, HTMLElement)));
    assertFalse(isVisible(getElement(offlineImgSelector)));

    await goOffline();

    // When offline, should show offline message.
    assertTrue(isVisible(strictQuery(
        offlineImgSelector, helpContentElement.shadowRoot, HTMLElement)));
    // Content not available image should be invisible.
    assertFalse(isVisible(getElement(noContentImgSelector)));
  });
});
