// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FilterDialogElement} from 'chrome://updater/event_list/filter_dialog/filter_dialog.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {whenCheck} from 'chrome://webui-test/test_util.js';

suite('FilterDialogElement', () => {
  let filterDialog: FilterDialogElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('renders correctly', () => {
    filterDialog = new FilterDialogElement();
    document.body.appendChild(filterDialog);
    assertTrue(filterDialog instanceof HTMLElement);
    assertEquals('FILTER-DIALOG', filterDialog.tagName);
  });

  test('positions dialog relative to parent by default', async () => {
    const parent = document.createElement('div');
    parent.style.position = 'absolute';
    parent.style.top = '100px';
    parent.style.left = '100px';
    parent.style.width = '100px';
    parent.style.height = '100px';
    document.body.appendChild(parent);

    filterDialog = new FilterDialogElement();
    parent.appendChild(filterDialog);

    const dialog = filterDialog.$.dialog;
    await whenCheck(dialog, () => dialog.style.top !== '');

    assertEquals('204px', dialog.style.top);
    assertEquals('100px', dialog.style.left);
  });

  test('positions dialog relative to anchorElement if set', async () => {
    const anchor = document.createElement('div');
    anchor.style.position = 'absolute';
    anchor.style.top = '200px';
    anchor.style.left = '200px';
    anchor.style.width = '50px';
    anchor.style.height = '50px';
    document.body.appendChild(anchor);

    filterDialog = new FilterDialogElement();
    filterDialog.anchorElement = anchor;
    document.body.appendChild(filterDialog);

    const dialog = filterDialog.$.dialog;
    await whenCheck(dialog, () => dialog.style.top !== '');

    assertEquals('254px', dialog.style.top);
    assertEquals('200px', dialog.style.left);
  });

  test('repositions dialog when anchorElement changes', async () => {
    const initialAnchor = document.createElement('div');
    initialAnchor.style.position = 'absolute';
    initialAnchor.style.top = '10px';
    initialAnchor.style.left = '10px';
    initialAnchor.style.width = '20px';
    initialAnchor.style.height = '20px';
    document.body.appendChild(initialAnchor);

    filterDialog = new FilterDialogElement();
    filterDialog.anchorElement = initialAnchor;
    document.body.appendChild(filterDialog);

    const dialog = filterDialog.$.dialog;
    await whenCheck(dialog, () => dialog.style.top !== '');

    assertEquals('34px', dialog.style.top);  // 10 + 20 + 4
    assertEquals('10px', dialog.style.left);

    const newAnchor = document.createElement('div');
    newAnchor.style.position = 'absolute';
    newAnchor.style.top = '300px';
    newAnchor.style.left = '300px';
    newAnchor.style.width = '40px';
    newAnchor.style.height = '40px';
    document.body.appendChild(newAnchor);

    filterDialog.anchorElement = newAnchor;
    await whenCheck(dialog, () => dialog.style.top === '344px');

    assertEquals('344px', dialog.style.top);  // 300 + 40 + 4
    assertEquals('300px', dialog.style.left);
  });

  test('repositions dialog on scroll', async () => {
    const anchor = document.createElement('div');
    anchor.style.position = 'absolute';
    anchor.style.top = '200px';
    anchor.style.left = '200px';
    anchor.style.width = '50px';
    anchor.style.height = '50px';
    document.body.appendChild(anchor);

    filterDialog = new FilterDialogElement();
    filterDialog.anchorElement = anchor;
    document.body.appendChild(filterDialog);

    const dialog = filterDialog.$.dialog;
    await whenCheck(dialog, () => dialog.style.top !== '');

    assertEquals('254px', dialog.style.top);
    assertEquals('200px', dialog.style.left);

    // Simulate scroll by moving the anchor and firing a scroll event.
    anchor.style.top = '100px';
    window.dispatchEvent(
        new CustomEvent('scroll', {bubbles: true, composed: true}));

    await whenCheck(dialog, () => dialog.style.top === '154px');

    assertEquals('154px', dialog.style.top);
    assertEquals('200px', dialog.style.left);
  });
});
