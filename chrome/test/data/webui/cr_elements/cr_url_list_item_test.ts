// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';

import type {CrUrlListItemElement} from 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {CrUrlListItemSize} from 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';
import {getTrustedHtml} from 'chrome://webui-test/trusted_html.js';

suite('CrUrlListItemTest', () => {
  let element: CrUrlListItemElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('cr-url-list-item');
    document.body.appendChild(element);
    return element.updateComplete;
  });

  test('TogglesBetweenIcons', async () => {
    const favicon = element.shadowRoot!.querySelector<HTMLElement>('.favicon')!;
    const folderIcon =
        element.shadowRoot!.querySelector<HTMLElement>('.folder-and-count')!;

    element.count = 4;
    await element.updateComplete;
    assertTrue(favicon.hasAttribute('hidden'));
    assertFalse(folderIcon.hasAttribute('hidden'));

    element.count = undefined;
    element.url = 'http://google.com';
    await element.updateComplete;
    assertFalse(favicon.hasAttribute('hidden'));
    assertEquals(
        getFaviconForPageURL('http://google.com', false),
        favicon.style.backgroundImage);
    assertTrue(folderIcon.hasAttribute('hidden'));
  });

  test('TruncatesAndDisplaysCount', async () => {
    const count = element.shadowRoot!.querySelector('.count')!;
    element.count = 11;
    await element.updateComplete;
    assertEquals('11', count.textContent);
    element.count = 2983;
    await element.updateComplete;
    assertEquals('99+', count.textContent);
  });

  test('SetsActiveClass', () => {
    assertFalse(element.classList.contains('active'));
    element.dispatchEvent(new PointerEvent('pointerdown'));
    assertTrue(element.classList.contains('active'));
    element.dispatchEvent(new PointerEvent('pointerup'));
    assertFalse(element.classList.contains('active'));

    element.dispatchEvent(new PointerEvent('pointerdown'));
    assertTrue(element.classList.contains('active'));
    element.dispatchEvent(new PointerEvent('pointerleave'));
    assertFalse(element.classList.contains('active'));
  });

  test('TogglesFolderIcon', async () => {
    element.url = '';
    element.imageUrls = ['http://google.com'];
    element.size = CrUrlListItemSize.COMPACT;
    const folderIcon =
        element.shadowRoot!.querySelector<HTMLElement>('.icon-folder-open');
    assertTrue(!!folderIcon);
    await element.updateComplete;
    assertFalse(isVisible(folderIcon));

    element.url = undefined;
    element.size = CrUrlListItemSize.LARGE;
    await element.updateComplete;
    assertFalse(isVisible(folderIcon));

    element.imageUrls = [];
    await element.updateComplete;
    assertTrue(isVisible(folderIcon));
  });

  test('DisplaysAlternateFolderIcon', async () => {
    document.body.innerHTML = getTrustedHtml(`<cr-url-list-item size="compact">
        <div id="test-icon" slot="folder-icon"></div>
       </cr-url-list-item>`);

    const urlListItemElement = document.body.querySelector('cr-url-list-item');
    assertTrue(!!urlListItemElement);
    await element.updateComplete;
    const slot = urlListItemElement.shadowRoot!.querySelector<HTMLSlotElement>(
        'slot[name="folder-icon"]');
    assertTrue(!!slot);
    const slotElements = slot.assignedElements();
    assertEquals(1, slotElements.length);
  });

  test('DisplaysMaxImageCount', async () => {
    element.imageUrls = [
      'http://www.first.com',
      'http://www.second.com',
      'http://www.third.com',
    ];
    await element.updateComplete;
    const imageElements =
        element.shadowRoot!.querySelectorAll<HTMLElement>('.folder-image');
    // No more than two images may be displayed for a folder.
    assertEquals(2, imageElements.length);
  });

  test('DisplaysImageAfterLoad', async () => {
    element.url = 'http://google.com';
    element.imageUrls = [
      'http://www.image.png',
    ];
    await microtasksFinished();
    const imageContainer =
        element.shadowRoot!.querySelector<HTMLElement>('.image-container');
    assertTrue(!!imageContainer);
    assertFalse(isVisible(imageContainer));

    const firstImage = element.shadowRoot!.querySelector('img');
    assertTrue(!!firstImage);
    firstImage.dispatchEvent(new Event('load'));
    await microtasksFinished();
    assertTrue(isVisible(imageContainer));

    // Changing imageUrls should reset the load logic
    element.imageUrls = [
      'http://www.image2.png',
    ];
    await microtasksFinished();
    assertFalse(isVisible(imageContainer));

    firstImage.dispatchEvent(new Event('load'));
    await microtasksFinished();
    assertTrue(isVisible(imageContainer));
  });

  test('HidesAndShowSuffix', async () => {
    const suffix = document.createElement('div');
    suffix.slot = 'suffix';
    suffix.innerText = 'Suffix text';
    element.appendChild(suffix);

    assertFalse(isVisible(suffix));
    element.classList.add('hovered');
    assertTrue(isVisible(suffix));
    element.classList.remove('hovered');
    assertFalse(isVisible(suffix));

    const focusOutlineManager = FocusOutlineManager.forDocument(document);
    focusOutlineManager.visible = true;
    assertFalse(isVisible(suffix));
    element.focus();
    assertTrue(isVisible(suffix));
    focusOutlineManager.visible = false;
    assertFalse(isVisible(suffix));

    element.alwaysShowSuffix = true;
    await element.updateComplete;
    assertTrue(isVisible(suffix));
  });

  test('SwitchesBetweenAnchorAndButton', async () => {
    assertFalse(isVisible(element.$.anchor));
    assertTrue(isVisible(element.$.button));
    assertDeepEquals(
        element.getBoundingClientRect(),
        element.$.button.getBoundingClientRect());
    element.asAnchor = true;
    await element.updateComplete;
    assertTrue(isVisible(element.$.anchor));
    assertFalse(isVisible(element.$.button));
    assertDeepEquals(
        element.getBoundingClientRect(),
        element.$.anchor.getBoundingClientRect());
  });

  test('SetsTargetForAnchor', async () => {
    element.asAnchor = true;
    await element.updateComplete;
    assertEquals('_self', element.$.anchor.target);
    element.asAnchorTarget = '_blank';
    await element.updateComplete;
    assertEquals('_blank', element.$.anchor.target);
  });

  test('PassesAriaProperties', async () => {
    element.title = 'My title';
    element.description = 'My description';
    await element.updateComplete;
    assertEquals('My title', element.$.anchor.ariaLabel);
    assertEquals('My description', element.$.anchor.ariaDescription);
    assertEquals('My title', element.$.button.ariaLabel);
    assertEquals('My description', element.$.button.ariaDescription);

    element.itemAriaLabel = 'My aria label';
    element.itemAriaDescription = 'My aria description';
    await element.updateComplete;
    assertEquals('My aria label', element.$.anchor.ariaLabel);
    assertEquals('My aria description', element.$.anchor.ariaDescription);
    assertEquals('My aria label', element.$.button.ariaLabel);
    assertEquals('My aria description', element.$.button.ariaDescription);
  });

  test('TransfersFocus', async () => {
    element.focus();
    assertEquals(element.$.button, getDeepActiveElement());

    element.asAnchor = true;
    element.url = 'http://google.com';
    await element.updateComplete;
    element.focus();
    assertEquals(element.$.anchor, getDeepActiveElement());
  });

  test('ContentSlotOverridesMetadata', async () => {
    element.title = 'My title';
    await element.updateComplete;
    assertTrue(isVisible(element.$.metadata));

    const slotchangeEvent = eventToPromise('slotchange', element.$.content);
    const contentSlot = document.createElement('div');
    contentSlot.slot = 'content';
    element.appendChild(contentSlot);
    await slotchangeEvent;
    assertFalse(isVisible(element.$.metadata));
  });

  test('BadgesSlot', async () => {
    assertFalse(isVisible(element.$.badgesContainer));

    const slotchangeEvent = eventToPromise('slotchange', element.$.badges);
    const contentSlot = document.createElement('div');
    contentSlot.slot = 'badges';
    element.appendChild(contentSlot);
    await slotchangeEvent;
    assertFalse(isVisible(element.$.badgesContainer));
  });
});
