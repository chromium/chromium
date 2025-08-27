// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {client, getBrowser, logMessage} from '../client.js';
import {$} from '../page_element_types.js';
import type {PageMetadata, Subscriber} from '/glic/glic_api/glic_api.js';

let subscription: Subscriber|null = null;

function unsubscribe() {
  if (subscription) {
    subscription.unsubscribe();
    subscription = null;
  }
}

function subscribe() {
  const tabId = $.pageMetadataTabsList.value;
  const names = $.pageMetadataNames.value.split(',').map(s => s.trim());
  if (!tabId || names.length === 0) {
    $.pageMetadataResult.value = 'Tab ID and names are required.';
    return;
  }

  unsubscribe();

  const metadataObservable = getBrowser()!.getPageMetadata!(tabId, names);
  if (!metadataObservable) {
    $.pageMetadataResult.value = 'Failed to get metadata observable.';
    return;
  }

  subscription = metadataObservable.subscribe((metadata: PageMetadata) => {
    $.pageMetadataResult.value = JSON.stringify(metadata, null, 2);
  });

  $.pageMetadataStatus.innerText = `Subscribed to tab ${tabId}`;
}

function refreshTabs(): Promise<void> {
  return new Promise(resolve => {
    const observable = getBrowser()!.getPinCandidates!({
      maxCandidates: 100,
    });
    const sub = observable.subscribe(candidates => {
      sub.unsubscribe();

      $.pageMetadataTabsList.innerHTML = '';
      for (const candidate of candidates) {
        const {tabId, title} = candidate.tabData;
        const option = document.createElement('option');
        option.value = tabId;
        option.textContent = `${title} (${tabId})`;
        $.pageMetadataTabsList.appendChild(option);
      }
      resolve();
    });
  });
}

function initPageMetadata() {
  if (!getBrowser()?.getPinnedTabs) {
    $.pageMetadataSection.disabled = true;
    $.pageMetadataStatus.innerText =
        'Multi-tab feature is disabled. Relaunch Chrome with ' +
        '--enable-features=GlicMultiTab to enable.';
    return;
  }
  logMessage('page_metadata.ts loaded');

  $.pageMetadataSubscribe.addEventListener('click', subscribe);

  $.pageMetadataUnsubscribe.addEventListener('click', () => {
    if (subscription) {
      unsubscribe();
      $.pageMetadataStatus.innerText = 'Unsubscribed';
    }
  });

  $.pageMetadataRefreshTabs.addEventListener('click', refreshTabs);

  $.pageMetadataOpenTestPage.addEventListener('click', async () => {
    const testPageUrl = new
        URL('/glic/test_client/metadata_test_page.html', window.location.href);
    const tabData = await getBrowser()!.createTab!(
        testPageUrl.href, {openInBackground: false});
    logMessage(`createTab done: ${JSON.stringify(tabData)}`);
    await refreshTabs();
    $.pageMetadataTabsList.value = tabData.tabId;
  });

  // Initial load
  refreshTabs();
}

client.getInitialized().then(initPageMetadata);
