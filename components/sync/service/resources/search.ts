// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_splitter/cr_splitter.js';

import {assert} from 'chrome://resources/js/assert.js';

import {decorateQuickQueryControls, SyncSearchManager} from './sync_search.js';

const submit = document.querySelector<HTMLButtonElement>('#sync-search-submit');
const query = document.querySelector<HTMLInputElement>('#sync-search-query');
const quickLinks =
    document.querySelectorAll<HTMLElement>('.sync-search-quicklink');
const status = document.querySelector<HTMLElement>('#sync-search-status');
const results = document.querySelector<HTMLElement>('#sync-results-list');
const detail = document.querySelector<HTMLElement>('#sync-result-details');
assert(submit && query && status && results && detail);

decorateQuickQueryControls(quickLinks, submit, query);
const manager = new SyncSearchManager(query, submit, status, results, detail);

// Add a way to override the data model for the sync results list for testing.
export function setupSyncResultsListForTest(data: object[]) {
  manager.setDataForTest(data);
}
