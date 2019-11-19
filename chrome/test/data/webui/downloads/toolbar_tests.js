// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SearchService} from 'chrome://downloads/downloads.js';

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

    PolymerTest.clearBody();
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

  test('toast is shown when clear all button clicked', () => {
    assertFalse(toastManager.isToastOpen);
    toolbar.hasClearableDownloads = true;
    toolbar.$$('#moreActionsMenu button').click();
    assertTrue(toastManager.isToastOpen);
    assertFalse(toastManager.isUndoButtonHidden);
  });
});
