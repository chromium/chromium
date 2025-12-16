// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {expect} from '//webui-test/chai.js';
import {FilterCategory} from 'chrome://updater/event_list/filter_bar.js';
import {TypeDialogElement} from 'chrome://updater/event_list/filter_dialog/type_dialog.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('TypeDialogElement', () => {
  let filterType: TypeDialogElement;

  setup(() => {
    filterType = new TypeDialogElement();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(filterType);
  });

  test('renders correctly', () => {
    expect(filterType instanceof HTMLElement).to.be.true;
    expect(filterType.tagName).to.equal('TYPE-DIALOG');
  });

  test('displays menu items', async () => {
    await microtasksFinished();
    const items = filterType.shadowRoot.querySelectorAll('.filter-menu-item');
    expect(items.length).to.equal(4);
    expect(items[0]!.textContent.trim()).to.equal('App');
    expect(items[1]!.textContent.trim()).to.equal('Event Type');
    expect(items[2]!.textContent.trim()).to.equal('Update Outcome');
    expect(items[3]!.textContent.trim()).to.equal('Date');
  });

  test('fires type-selection-changed event on click', async () => {
    await microtasksFinished();
    let capturedEvent: CustomEvent<FilterCategory>|null = null;
    filterType.addEventListener('type-selection-changed', (e: Event) => {
      capturedEvent = e as CustomEvent<FilterCategory>;
    });

    const items = filterType.shadowRoot.querySelectorAll<HTMLElement>(
        '.filter-menu-item');
    items[0]!.click();
    await microtasksFinished();

    expect(capturedEvent).to.not.be.null;
    expect(capturedEvent!.detail).to.equal(FilterCategory.APP);
  });
});
