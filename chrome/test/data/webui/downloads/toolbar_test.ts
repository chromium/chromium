// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrToastManagerElement, DownloadsToolbarElement} from 'chrome://downloads/downloads.js';
import {SearchService} from 'chrome://downloads/downloads.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createDownload} from './test_support.js';

suite('toolbar tests', function() {
  let toolbar: DownloadsToolbarElement;
  let toastManager: CrToastManagerElement;

  setup(function() {
    class TestSearchService extends SearchService {
      override loadMore() { /* Prevent chrome.send(). */
      }
    }

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    toolbar = document.createElement('downloads-toolbar');
    SearchService.setInstance(new TestSearchService());
    document.body.appendChild(toolbar);

    toastManager = document.createElement('cr-toast-manager');
    document.body.appendChild(toastManager);
  });

  test('search starts spinner', function() {
    toolbar.$.toolbar.dispatchEvent(new CustomEvent(
        'search-changed', {composed: true, bubbles: true, detail: 'a'}));
    assertTrue(toolbar.spinnerActive);

    // Pretend the manager got results and set this to false.
    toolbar.spinnerActive = false;

    toolbar.$.toolbar.dispatchEvent(new CustomEvent(
        'search-changed', {composed: true, bubbles: true, detail: 'a '}));
    assertFalse(toolbar.spinnerActive);
  });

  test('clear all shown/hidden', async () => {
    const clearAll = toolbar.$.clearAll;
    assertTrue(clearAll.hidden);
    toolbar.hasClearableDownloads = true;
    await microtasksFinished();
    assertFalse(clearAll.hidden);

    toolbar.$.toolbar.getSearchField().setValue('test');
    await microtasksFinished();
    assertTrue(clearAll.hidden);
  });

  test('clear all event fired', () => {
    assertFalse(toastManager.isToastOpen);
    assertFalse(toastManager.slottedHidden);
    toolbar.hasClearableDownloads = true;
    toolbar.$.clearAll.click();
    assertTrue(toastManager.isToastOpen);
    assertTrue(toastManager.slottedHidden);
  });

  test('undo is not shown when removing only dangerous items', () => {
    toolbar.items = [
      createDownload({isDangerous: true}),
      createDownload({isInsecure: true}),
    ];
    toastManager.show('', /* hideSlotted= */ false);
    assertFalse(toastManager.slottedHidden);
    toolbar.hasClearableDownloads = true;
    toolbar.$.clearAll.click();
    assertTrue(toastManager.isToastOpen);
    assertTrue(toastManager.slottedHidden);
  });

  test('undo is shown when removing items', () => {
    toolbar.items = [
      createDownload(),
      createDownload({isDangerous: true}),
      createDownload({isInsecure: true}),
    ];
    toastManager.show('', /* hideSlotted= */ true);
    assertTrue(toastManager.slottedHidden);
    toolbar.hasClearableDownloads = true;
    toolbar.$.clearAll.click();
    assertTrue(toastManager.isToastOpen);
    assertFalse(toastManager.slottedHidden);
  });
});
