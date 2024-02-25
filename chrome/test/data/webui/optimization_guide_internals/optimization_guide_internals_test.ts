// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('OptimizationGuideInternalsTest', function() {
  test('EmptyTest', function() {
    // Nothing to do here. This is to support a test that checks the state of
    // the logger before and after opening the page.
  });

  test('InternalsPageOpen', function() {
    return new Promise(resolve => {
      setInterval(() => {
        const container = document.getElementById('log-message-container');
        assertTrue(!!container);
        if (container.children[0]!.childElementCount > 2) {
          resolve(true);
        }
      }, 500);
    });
  });

  test('InternalsModelsPageOpen', function() {
    window.history.replaceState({}, '', '#models');
    window.dispatchEvent(new CustomEvent('hashchange'));

    const containerHasChildren = new Promise(resolve => {
      setTimeout(() => {
        const container =
            document.getElementById('downloaded-models-container');
        assertTrue(!!container);
        if (container.children[0]!.childElementCount > 0) {
          resolve(true);
        }
      }, 500);
    });

    const tableRowExists = new Promise(resolve => {
      setTimeout(() => {
        const tableRow =
            document.getElementById('OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD');
        if (tableRow) {
          resolve(true);
        }
      }, 500);
    });

    return Promise.all([containerHasChildren, tableRowExists]);
  });


  test('InternalsClientIdsPageOpen', function() {
    window.history.replaceState({}, '', '#client-ids');
    window.dispatchEvent(new CustomEvent('hashchange'));

    const containerHasChildren = new Promise(resolve => {
      setTimeout(() => {
        const container =
            document.getElementById('logged-client-ids-container');
        assertTrue(!!container);
        if (container.children[0]!.childElementCount > 0) {
          resolve(true);
        }
      }, 500);
    });

    return Promise.all([containerHasChildren]);
  });
});
