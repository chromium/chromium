// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://organizer-panel.top-chrome/organizer_panel.js';

import type {OrganizerListSectionItemElement} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('OrganizerListSectionItemTest', () => {
  let listItem: OrganizerListSectionItemElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    listItem = document.createElement('organizer-list-section-item');
    document.body.appendChild(listItem);
    await microtasksFinished();
  });

  test('renders title and description', async () => {
    listItem.item = {
      title: 'Google Search',
      description: ['google.com', '5 mins ago'],
    };
    await microtasksFinished();

    const crUrlListItem = listItem.$.crUrlListItem;
    assertTrue(!!crUrlListItem);
    assertEquals('Google Search', crUrlListItem.title);
    assertEquals('google.com · 5 mins ago', crUrlListItem.description);
  });

  test('renders prefix icon with URL', async () => {
    listItem.item = {
      title: 'Google Search',
      prefixIcon: {
        urls: ['https://google.com'],
      },
    };
    await microtasksFinished();

    const crUrlListItem = listItem.$.crUrlListItem;
    assertTrue(!!crUrlListItem);
    assertEquals('https://google.com', crUrlListItem.url);
  });

  test('renders prefix icon with custom element', async () => {
    listItem.item = {
      title: 'Tab Group',
      prefixIcon: {
        element: html`<span id="customGroupIcon">Group</span>`,
      },
    };
    await microtasksFinished();

    const crUrlListItem = listItem.$.crUrlListItem;
    assertTrue(!!crUrlListItem);
    const customIcon = crUrlListItem.querySelector('#customGroupIcon');
    assertTrue(!!customIcon);
    assertEquals('Group', customIcon.textContent);
  });
});
