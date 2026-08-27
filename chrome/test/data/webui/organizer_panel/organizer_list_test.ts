// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://organizer-panel.top-chrome/organizer_panel.js';

import type {OrganizerListElement} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSectionDelegate} from './test_section_delegate.js';

suite('OrganizerListTest', () => {
  let list: OrganizerListElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    list = document.createElement('organizer-list');
    document.body.appendChild(list);
    await microtasksFinished();
  });

  test('renders sections for delegates', async () => {
    list.sectionDelegates = [
      new TestSectionDelegate('Section 1'),
    ];
    await microtasksFinished();

    const sections = list.shadowRoot.querySelectorAll('organizer-list-section');
    assertEquals(1, sections.length);
    assertTrue(!!sections[0]!.delegate);
    assertEquals('Section 1', sections[0]!.delegate.getHeader());

    const dividers = list.shadowRoot.querySelectorAll('.divider');
    assertEquals(0, dividers.length);
  });

  test('renders dividers between sections', async () => {
    list.sectionDelegates = [
      new TestSectionDelegate('Section 1'),
      new TestSectionDelegate('Section 2'),
      new TestSectionDelegate('Section 3'),
    ];
    await microtasksFinished();

    const sections = list.shadowRoot.querySelectorAll('organizer-list-section');
    assertEquals(3, sections.length);

    const dividers = list.shadowRoot.querySelectorAll('.divider');
    assertEquals(2, dividers.length);
  });
});
