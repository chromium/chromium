// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../mojo_webui_test_support.js';

import {SearchService} from 'chrome://downloads/downloads.js';
import {createDownload} from 'chrome://test/downloads/test_support.js';

suite('toolbar tests', function() {
  /** @type {!downloads.Toolbar} */
  let toolbar;

  /** @type {!CrToastManagerElement} */
  let toastManager;

  setup(function() {
    class TestSearchService extends SearchService {
      loadMore() { /* Prevent chrome.send(). */
      }
    }

    document.body.innerHTML = '';
    toolbar = document.createElement('downloads-toolbar');
    SearchService.instance_ = new TestSearchService;
    document.body.appendChild(toolbar);

    toastManager = document.createElement('cr-toast-manager');
    document.body.appendChild(toastManager);
  });

  test('resize closes more options menu', function() {
    toolbar.$.moreActions.click();
    assertTrue(toolbar.$.moreActionsMenu.open);

    window.dispatchEvent(new CustomEvent('resize'));
    assertFalse(toolbar.$.moreActionsMenu.open);
  });

  test('search starts spinner', function() {
    toolbar.$.toolbar.fire('search-changed', 'a');
    assertTrue(toolbar.spinnerActive);

    // Pretend the manager got results and set this to false.
    toolbar.spinnerActive = false;

    toolbar.$.toolbar.fire('search-changed', 'a ');  // Same term plus a space.
    assertFalse(toolbar.spinnerActive);
  });

  test('clear all shown/hidden', () => {
    const clearAll = toolbar.$$('#moreActionsMenu button');
    assertTrue(clearAll.hidden);
    toolbar.hasClearableDownloads = true;
    assertFalse(clearAll.hidden);
    toolbar.$.toolbar.getSearchField().setValue('test');
    assertTrue(clearAll.hidden);
  });

  test('clear all event fired', () => {
    assertFalse(toastManager.isToastOpen);
    assertFalse(toastManager.slottedHidden);
    toolbar.hasClearableDownloads = true;
    toolbar.$$('#moreActionsMenu button').click();
    assertTrue(toastManager.isToastOpen);
    assertTrue(toastManager.slottedHidden);
  });

  test('undo is not shown when removing only dangerous items', () => {
    toolbar.items = [
      createDownload({isDangerous: true}),
      createDownload({isMixedContent: true})
    ];
    toastManager.show('', /* hideSlotted= */ false);
    assertFalse(toastManager.slottedHidden);
    toolbar.hasClearableDownloads = true;
    toolbar.$$('#moreActionsMenu button').click();
    assertTrue(toastManager.isToastOpen);
    assertTrue(toastManager.slottedHidden);
  });

  test('undo is shown when removing items', () => {
    toolbar.items = [
      createDownload(), createDownload({isDangerous: true}),
      createDownload({isMixedContent: true})
    ];
    toastManager.show('', /* hideSlotted= */ true);
    assertTrue(toastManager.slottedHidden);
    toolbar.hasClearableDownloads = true;
    toolbar.$$('#moreActionsMenu button').click();
    assertTrue(toastManager.isToastOpen);
    assertFalse(toastManager.slottedHidden);
  });
});
