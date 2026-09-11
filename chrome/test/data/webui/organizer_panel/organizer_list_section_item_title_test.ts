// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://organizer-panel.top-chrome/organizer_panel.js';

import type {OrganizerListSectionItemTitleElement} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

const TEST_TITLE_1 = 'Google Search';
const TEST_TITLE_2 = 'YouTube';
const TEST_TITLE_3 = 'Chromium';

suite('OrganizerListSectionItemTitleTest', () => {
  let element: OrganizerListSectionItemTitleElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('organizer-list-section-item-title');
    document.body.appendChild(element);
    await microtasksFinished();
  });

  test('renders single title without separator', async () => {
    element.titleParts = [TEST_TITLE_1];
    await microtasksFinished();

    const titleParts =
        element.$.titleParts.querySelectorAll<HTMLElement>('.title-part');
    const separators =
        element.$.titleParts.querySelectorAll<HTMLElement>('.separator');
    assertEquals(1, titleParts.length);
    assertEquals(0, separators.length);
    assertEquals(TEST_TITLE_1, titleParts[0]!.textContent.trim());
    assertEquals(TEST_TITLE_1, titleParts[0]!.getAttribute('title'));
  });

  test('renders multiple titles separated by vertical bar', async () => {
    element.titleParts = [TEST_TITLE_1, TEST_TITLE_2, TEST_TITLE_3];
    await microtasksFinished();

    const titleParts =
        element.$.titleParts.querySelectorAll<HTMLElement>('.title-part');
    const separators =
        element.$.titleParts.querySelectorAll<HTMLElement>('.separator');
    assertEquals(3, titleParts.length);
    assertEquals(2, separators.length);
    assertEquals(TEST_TITLE_1, titleParts[0]!.textContent.trim());
    assertEquals(TEST_TITLE_2, titleParts[1]!.textContent.trim());
    assertEquals(TEST_TITLE_3, titleParts[2]!.textContent.trim());
    assertEquals('|', separators[0]!.textContent.trim());
    assertEquals('|', separators[1]!.textContent.trim());
  });

  test('updates when titleParts change', async () => {
    element.titleParts = [TEST_TITLE_1];
    await microtasksFinished();

    assertEquals(
        1,
        element.$.titleParts.querySelectorAll<HTMLElement>('.title-part')
            .length);

    element.titleParts = [TEST_TITLE_2, TEST_TITLE_3];
    await microtasksFinished();

    const titleParts =
        element.$.titleParts.querySelectorAll<HTMLElement>('.title-part');
    assertEquals(2, titleParts.length);
    assertEquals(TEST_TITLE_2, titleParts[0]!.textContent.trim());
    assertEquals(TEST_TITLE_3, titleParts[1]!.textContent.trim());
  });
});
