// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ObservableValue, PinCandidate, Subscriber, TabContextResult, TabData} from '/glic/glic_api/glic_api.js';
import {DEFAULT_PDF_SIZE_LIMIT} from '/glic/glic_api/glic_api.js';

import {client, getBrowser, logMessage} from '../client.js';
import {$} from '../page_element_types.js';

interface PinnedTabState {
  contextVersion: number;
  tabData: TabData;
  errorReason: string;
}

interface PinnedTabStateUpdate {
  tabId: string;
  errorReason?: string;
  tabContext?: TabContextResult;
}

let state: PinnedTabState[] = [];
let pinCandidateSubscriber: Subscriber|undefined;
let pinCandidateObservable: ObservableValue<PinCandidate[]>|undefined;

function updateStateWithTabData(tabData: TabData[]) {
  state = state.filter((x) => {
    for (const tab of tabData) {
      if (tab.tabId === x.tabData.tabId) {
        return true;
      }
    }
    return false;
  });

  for (const tab of tabData) {
    let updated = false;
    for (const s of state) {
      if (s.tabData.tabId === tab.tabId) {
        s.tabData = tab;
        updated = true;
        break;
      }
    }
    if (!updated) {
      state.push({
        contextVersion: 0,
        tabData: tab,
        errorReason: '',
      });
    }
  }
}

function updateStateWithTabContextUpdate(update: PinnedTabStateUpdate): void {
  for (const tabState of state) {
    if (tabState.tabData.tabId === update.tabId) {
      if (update.errorReason) {
        tabState.errorReason = update.errorReason;
      }
      if (update.tabContext) {
        if (update.tabContext.webPageData ||
            update.tabContext.viewportScreenshot ||
            update.tabContext.pdfDocumentData ||
            update.tabContext.annotatedPageData) {
          tabState.contextVersion += 1;
          tabState.errorReason = '';
        }
      }
      break;
    }
  }
}

async function fetchPinnedTabState(
    tabState: PinnedTabState,
    observableTabOnly: boolean): Promise<PinnedTabStateUpdate> {
  const update: PinnedTabStateUpdate = {
    tabId: tabState.tabData.tabId,
  };
  if (!tabState.tabData.isObservable && observableTabOnly) {
    return update;
  }

  try {
    const viewportScreenshot =
        observableTabOnly || $.multiTabFetchScreenshot.checked;
    const annotatedPageContent = true;
    const pdfData = true;
    const pdfSizeLimit = DEFAULT_PDF_SIZE_LIMIT;
    const maxMetaTags = 100;
    update.tabContext =
        await getBrowser()!.getContextFromTab!(tabState.tabData.tabId, {
          viewportScreenshot,
          annotatedPageContent,
          pdfData,
          pdfSizeLimit,
          maxMetaTags,
        });
    logMessage(`Pinned tab context: ` + JSON.stringify(update));
  } catch (e: any) {
    logMessage(`Failed to grab pinned tab context: ` + JSON.stringify(e));
    update.errorReason = e.message;
  }
  return update;
}

function updateUi() {
  // Could be incremental; recreating for convenience.
  while ($.pinnedTabs.childNodes.length > 0) {
    $.pinnedTabs.removeChild($.pinnedTabs.firstChild!);
  }

  state.forEach((tabState: PinnedTabState) => {
    const li = document.createElement('LI');

    // Favicon.
    if (tabState.tabData.favicon) {
      const favicon = document.createElement('IMG') as HTMLImageElement;
      tabState.tabData.favicon().then((blob) => {
        if (blob) {
          favicon.src = URL.createObjectURL(blob);
        }
      });
      favicon.classList.add('favicon');
      li.appendChild(favicon);
    }

    // URL / Title.
    const a = document.createElement('A') as HTMLAnchorElement;
    a.href = tabState.tabData.url;
    a.innerText = tabState.tabData.title || tabState.tabData.url;
    li.appendChild(a);

    // Version.
    const version = document.createElement('SPAN');
    if (tabState.errorReason) {
      version.innerText = tabState.errorReason;
    } else {
      version.innerText = `-v${tabState.contextVersion}`;
    }
    li.appendChild(version);

    // Unpin.
    const button = document.createElement('BUTTON') as HTMLButtonElement;
    button.innerText = '❌';
    const clickHandler = ((tabId: string) => {
      return async () => {
        await getBrowser()!.unpinTabs!([tabId]);
      };
    })(tabState.tabData.tabId);
    button.addEventListener('click', clickHandler);
    li.appendChild(button);

    const pullButton = document.createElement('BUTTON') as HTMLButtonElement;
    pullButton.innerText = '⬆️ ';
    const pullClickHandler = ((tabState: PinnedTabState) => {
      return async () => {
        updateStateWithTabContextUpdate(
            await fetchPinnedTabState(tabState, false));
        await updateUi();
      };
    })(tabState);
    pullButton.addEventListener('click', pullClickHandler);
    li.appendChild(pullButton);

    $.pinnedTabs.appendChild(li);
  });
  if (state.length >= client.getMaxPinnedTabs()) {
    $.pinFocusedTab.setAttribute('disabled', '');
  } else {
    $.pinFocusedTab.removeAttribute('disabled');
  }
}

function clearCandidates() {
  while ($.shareCandidates.childNodes.length > 0) {
    $.shareCandidates.removeChild($.shareCandidates.firstChild!);
  }
}

function replaceCandidates(candidates: PinCandidate[]) {
  // Could be incremental; recreating for convenience.
  clearCandidates();

  for (const candidate of candidates) {
    const li = document.createElement('LI');

    // Favicon.
    if (candidate.tabData.favicon) {
      const favicon = document.createElement('IMG') as HTMLImageElement;
      candidate.tabData.favicon().then((blob: Blob|undefined) => {
        if (blob) {
          favicon.src = URL.createObjectURL(blob);
        }
      });
      favicon.classList.add('favicon');
      li.appendChild(favicon);
    }

    // Favicon URL.
    if (candidate.tabData.faviconUrl) {
      const faviconUrl = document.createElement('IMG') as HTMLImageElement;
      faviconUrl.src = candidate.tabData.faviconUrl;
      faviconUrl.classList.add('favicon');
      li.appendChild(faviconUrl);
    }

    // URL / Title.
    const a = document.createElement('A') as HTMLAnchorElement;
    a.href = candidate.tabData.url;
    a.innerText = candidate.tabData.title || candidate.tabData.url;
    li.appendChild(a);

    // Share
    const button = document.createElement('BUTTON') as HTMLButtonElement;
    button.innerText = 'Share';
    const clickHandler = async () => {
      await getBrowser()!.pinTabs!([candidate.tabData.tabId]);
    };
    button.addEventListener('click', clickHandler);
    li.appendChild(button);

    $.shareCandidates.appendChild(li);
  }
}

async function fetchCandidates() {
  if (pinCandidateSubscriber) {
    pinCandidateSubscriber.unsubscribe();
    pinCandidateSubscriber = undefined;
  }
  if (!pinCandidateObservable) {
    pinCandidateObservable = getBrowser()!.getPinCandidates!({
      maxCandidates: 10,
      query: $.shareCandidateQuery.value,
    });
  }
  pinCandidateSubscriber = pinCandidateObservable!.subscribe(replaceCandidates);
}

client.getInitialized().then(async () => {
  logMessage('Detected client initialized');

  const pinnedTabs = await getBrowser()!.getPinnedTabs?.();
  if (!pinnedTabs) {
    logMessage('Feature is disabled, bailing');
    $.multiTabSection.style = 'display:none';
    return;
  }

  client.setMaxPinnedTabs(await getBrowser()!.setMaximumNumberOfPinnedTabs!
                          (client.getMaxPinnedTabs()));

  $.pinFocusedTab.addEventListener('click', async () => {
    const currentTabId = client.getCurrentTabId();
    if (!currentTabId) {
      return;
    }
    try {
      await getBrowser()!.pinTabs!([currentTabId]);
    } catch (e: any) {
      logMessage(`pinning failed: ${e.message}`);
    }
  });

  $.unpin.addEventListener('click', async () => {
    await getBrowser()!.unpinAllTabs!();
  });

  pinnedTabs?.subscribe(async (tabData: TabData[]) => {
    updateStateWithTabData(tabData);
    await updateUi();
    logMessage(`Pinned tabs (${tabData.length}) updated to:`);
    for (const tab of tabData) {
      logMessage(tab.url + ', observable: ' + tab.isObservable);
    }
  });

  $.fetchPinned.addEventListener('click', async () => {
    const promises = [];
    for (const tabState of state) {
      promises.push(fetchPinnedTabState(tabState, true));
    }
    const updates = await Promise.all(promises);
    for (const update of updates) {
      updateStateWithTabContextUpdate(update);
    }
    await updateUi();
  });

  $.shareCandidateQuery.addEventListener('input', () => {
    pinCandidateSubscriber?.unsubscribe();
    pinCandidateSubscriber = undefined;
    pinCandidateObservable = undefined;
    if ($.enableShareCandidates.checked) {
      fetchCandidates();
    }
  });

  $.enableShareCandidates.addEventListener('click', () => {
    if ($.enableShareCandidates.checked) {
      fetchCandidates();
    } else {
      pinCandidateSubscriber?.unsubscribe();
      pinCandidateSubscriber = undefined;
    }
  });
});
