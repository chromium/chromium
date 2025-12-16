// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {expect} from '//webui-test/chai.js';
import {FilterDialogElement} from 'chrome://updater/event_list/filter_dialog/filter_dialog.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('FilterDialogElement', () => {
  let filterDialog: FilterDialogElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('renders correctly', () => {
    filterDialog = new FilterDialogElement();
    document.body.appendChild(filterDialog);
    expect(filterDialog instanceof HTMLElement).to.be.true;
    expect(filterDialog.tagName).to.equal('FILTER-DIALOG');
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

    await microtasksFinished();

    const dialog = filterDialog.$.dialog;
    expect(dialog.style.top).to.equal('204px');
    expect(dialog.style.left).to.equal('100px');
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

    await microtasksFinished();

    const dialog = filterDialog.$.dialog;
    expect(dialog.style.top).to.equal('254px');
    expect(dialog.style.left).to.equal('200px');
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

    await microtasksFinished();

    let dialog = filterDialog.$.dialog;
    expect(dialog.style.top).to.equal('34px');  // 10 + 20 + 4
    expect(dialog.style.left).to.equal('10px');

    const newAnchor = document.createElement('div');
    newAnchor.style.position = 'absolute';
    newAnchor.style.top = '300px';
    newAnchor.style.left = '300px';
    newAnchor.style.width = '40px';
    newAnchor.style.height = '40px';
    document.body.appendChild(newAnchor);

    filterDialog.anchorElement = newAnchor;
    await microtasksFinished();

    dialog = filterDialog.$.dialog;
    expect(dialog.style.top).to.equal('344px');  // 300 + 40 + 4
    expect(dialog.style.left).to.equal('300px');
  });
});
