// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://organizer-panel.top-chrome/organizer_panel.js';

import type {OrganizerListSectionHeaderElement} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('OrganizerListSectionHeaderTest', () => {
  let header: OrganizerListSectionHeaderElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    header = document.createElement('organizer-list-section-header');
    header.textContent = 'Open Tabs';
    header.expanded = true;
    document.body.appendChild(header);
    await microtasksFinished();
  });

  test('toggles expanded state when expand button is clicked', async () => {
    const expandButton = header.$.expandButton;
    const expandIcon = expandButton.$.icon;

    assertTrue(header.expanded);
    assertTrue(expandButton.expanded);
    assertEquals('cr:keyboard-arrow-up', expandIcon.ironIcon);
    assertEquals('true', expandIcon.getAttribute('aria-expanded'));

    let whenExpandedChanged = eventToPromise<CustomEvent<{value: boolean}>>(
        'expanded-changed', header);
    expandButton.click();
    let event = await whenExpandedChanged;
    await microtasksFinished();

    assertFalse(event.detail.value);
    assertFalse(header.expanded);
    assertFalse(expandButton.expanded);
    assertEquals('cr:keyboard-arrow-down', expandIcon.ironIcon);
    assertEquals('false', expandIcon.getAttribute('aria-expanded'));

    whenExpandedChanged = eventToPromise<CustomEvent<{value: boolean}>>(
        'expanded-changed', header);
    expandButton.click();
    event = await whenExpandedChanged;
    await microtasksFinished();

    assertTrue(event.detail.value);
    assertTrue(header.expanded);
    assertTrue(expandButton.expanded);
    assertEquals('cr:keyboard-arrow-up', expandIcon.ironIcon);
    assertEquals('true', expandIcon.getAttribute('aria-expanded'));
  });

  test(
      'hides expand icon and disables expand button when disabled',
      async () => {
        const expandButton = header.$.expandButton;
        const expandIcon = expandButton.$.icon;

        assertFalse(expandButton.disabled);
        assertEquals('flex', getComputedStyle(expandIcon).display);

        header.disabled = true;
        await microtasksFinished();

        assertTrue(expandButton.disabled);
        assertEquals('none', getComputedStyle(expandIcon).display);
      });
});
