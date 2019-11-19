// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// require: cr/ui.js
// require: util.js

cr.ui.decorate('#sync-results-splitter', cr.ui.Splitter);

chrome.sync.decorateQuickQueryControls(
  document.getElementsByClassName('sync-search-quicklink'),
  /** @type {!HTMLButtonElement} */ ($('sync-search-submit')),
  /** @type {!HTMLInputElement} */ ($('sync-search-query')));

chrome.sync.decorateSearchControls(
  /** @type {!HTMLInputElement} */ ($('sync-search-query')),
  /** @type {!HTMLButtonElement} */ ($('sync-search-submit')),
  getRequiredElement('sync-search-status'),
  getRequiredElement('sync-results-list'),
  /** @type {!HTMLPreElement} */ ($('sync-result-details')));
