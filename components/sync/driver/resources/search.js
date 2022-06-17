// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_splitter/cr_splitter.js';

import {$, getRequiredElement} from 'chrome://resources/js/util.m.js';

import {decorateQuickQueryControls, SyncSearchManager} from './sync_search.js';

decorateQuickQueryControls(
    document.getElementsByClassName('sync-search-quicklink'),
    /** @type {!HTMLButtonElement} */ ($('sync-search-submit')),
    /** @type {!HTMLInputElement} */ ($('sync-search-query')));

const manager = new SyncSearchManager(
    /** @type {!HTMLInputElement} */ ($('sync-search-query')),
    /** @type {!HTMLButtonElement} */ ($('sync-search-submit')),
    getRequiredElement('sync-search-status'),
    getRequiredElement('sync-results-list'),
    /** @type {!HTMLPreElement} */ ($('sync-result-details')));

// Add a way to override the data model for the sync results list for testing.
window.setupSyncResultsListForTest = function(data) {
  manager.setDataForTest(data);
};
