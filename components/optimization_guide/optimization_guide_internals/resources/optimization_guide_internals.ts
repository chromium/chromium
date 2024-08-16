// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_tab_box/cr_tab_box.js';

import {assert} from 'chrome://resources/js/assert.js';
import {$, getRequiredElement} from 'chrome://resources/js/util.js';
import type {Time} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import type {DownloadedModelInfo, LoggedClientIds} from './optimization_guide_internals.mojom-webui.js';
import {PageHandlerFactory} from './optimization_guide_internals.mojom-webui.js';
import {OptimizationGuideInternalsBrowserProxy} from './optimization_guide_internals_browser_proxy.js';

// Contains all the log events received when the internals page is open.
const logMessages: Array<{
  eventTime: string,
  logSource: string,
  sourceLocation: string,
  message: string,
}> = [];

// Type for a string filtering function.
type StringFilterPredicate = (s: string) => boolean;

/**
 * Class to apply filter functionality and updated related UI.
 */
class TableFilter {
  // Delay between input change to filter application, in ms.
  static readonly FILTER_DELAY_MS: number = 500;

  // Main <table> element for filtering. First row is assumed to be the title,
  readonly table: HTMLTableElement;

  // The text <input> element to get "include" filter text.
  readonly includeInput: HTMLInputElement;

  // The text <input> element to get "exclude" filter text.
  readonly excludeInput: HTMLInputElement;

  // The <span> element to output filter stats.
  readonly filterStatsSpan: HTMLSpanElement;

  // Assuming same number of cells in each row, list of indices of cells in each
  // row to examine for filtering. These correspond to <th> elements with
  // "filterable" in its classList.
  readonly filterCellIndices: number[];

  // Filter function for "include" filtering. Null if unspecified.
  includeFun: StringFilterPredicate|null;

  // Filter function for "exclude" filtering. Null if unspecified.
  excludeFun: StringFilterPredicate|null;

  // Id to setTimeout() for filter delay, so the timeout can be detected and
  // cancelled. Null if no timeout is live.
  filterDelayTimeoutId: number|null;

  // Total number of rows examined by the filter.
  numRows: number;

  // Total number of rows being show after filtering.
  numShown: number;

  /**
   * @param {!HTMLTableElement} table
   * @param {!HTMLInputElement} includeInput
   * @param {!HTMLInputElement!} excludeInput
   * @param {!HTMLSpanElement} filterStatsSpan
   */
  constructor(
      table: HTMLTableElement, includeInput: HTMLInputElement,
      excludeInput: HTMLInputElement, filterStatsSpan: HTMLSpanElement) {
    this.table = table;
    this.includeInput = includeInput;
    this.excludeInput = excludeInput;
    this.filterStatsSpan = filterStatsSpan;

    this.filterCellIndices = [];
    this.readFilterCellIndices();

    this.includeFun = this.readFilter(this.includeInput);
    this.excludeFun = this.readFilter(this.excludeInput);
    this.filterDelayTimeoutId = null;
    this.numRows = 0;
    this.numShown = 0;

    this.includeInput.addEventListener('input', (e) => this.triggerUpdate(e));
    this.excludeInput.addEventListener('input', (e) => this.triggerUpdate(e));
  }

  readFilterCellIndices() {
    this.filterCellIndices.length = 0;
    const headers = this.table.rows[0]!.cells;
    for (const [idx, header] of Array.from(headers).entries()) {
      if (header.classList.contains('filterable')) {
        this.filterCellIndices.push(idx);
      }
    }
  }

  /**
   * Reads filter text and returns a filter predicate, or null if no filter.
   * @param {!HTMLInputElement} input
   */
  readFilter(input: HTMLInputElement): StringFilterPredicate|null {
    const t = input.value;
    return (t === '') ? null : ((s: string) => (s.indexOf(t) >= 0));
  }

  /**
   * Returns whether the provided <tr> element should be shown.
   * @param {!HTMLTableElement} row
   */
  shouldRowBeShown(row: HTMLTableRowElement) {
    // Perform exclusion first since it has higher priority than inclusion.
    if (this.excludeFun != null) {
      for (const idx of this.filterCellIndices) {
        const text = row.cells[idx]?.textContent ?? '';
        if (this !.excludeFun(text)) {
          return false;
        }
      }
    }
    if (this.includeFun != null) {
      for (const idx of this.filterCellIndices) {
        const text = row.cells[idx]?.textContent ?? '';
        if (this !.includeFun(text)) {
          return true;
        }
      }
      return false;
    }
    return true;
  }

  /**
   * Shows or hides a <tr> element depending on its content.
   * @param {!HTMLTableRowElement} row
   * @return Whether the element is shown.
   */
  applyFilterToRow(row: HTMLTableRowElement): boolean {
    const shouldShow = this.shouldRowBeShown(row);
    row.classList.toggle('hidden', !shouldShow);
    return shouldShow;
  }

  /** Updates `filterStatsSpan` content to show filter stats. */
  writeFilterStats() {
    this.filterStatsSpan.textContent = `${this.numShown} / ${this.numRows}`;
  }

  /**
   * Visits every row (except the first, which is the titles) of `this.table`
   * and shows and hides it. Displays the number of hidden rows (as negative
   * value) in `filterStatsSpan`.
   */
  readAndApplyAllFilters() {
    this.readFilterCellIndices();
    this.includeFun = this.readFilter(this.includeInput);
    this.excludeFun = this.readFilter(this.excludeInput);
    this.numRows = 0;
    this.numShown = 0;
    let isTitle = true;
    for (const row of this.table.rows) {
      if (isTitle) {
        isTitle = false;
      } else {
        this.numShown += Number(this.applyFilterToRow(row));
        ++this.numRows;
      }
    }
    this.writeFilterStats();
  }

  /**
   * Applies the filter to a newly added row, and updates filter stats.
   * @param {!HTMLTableRowElement} row
   */
  filterNewRow(row: HTMLTableRowElement) {
    this.numShown += Number(this.applyFilterToRow(row));
    ++this.numRows;
    this.writeFilterStats();
  }

  /**
   * [Re]triggers timer to call readAndApplyAllFilters() after waiting for a
   * delay that lasts `FILTER_DELAY_MS`. Debouncing is applied.
   * @param {!Event} e
   */
  triggerUpdate(e: Event) {
    const elt = e!.target as HTMLElement;
    // Debounce: New trigger cancels an existing trigger's timeout.
    if (this.filterDelayTimeoutId != null) {
      clearTimeout(this.filterDelayTimeoutId);
      this.filterDelayTimeoutId = null;
    }
    elt.classList.add('input-dirty');
    this.filterDelayTimeoutId = setTimeout(() => {
      this.filterDelayTimeoutId = null;
      this.includeInput.classList.remove('input-dirty');
      this.excludeInput.classList.remove('input-dirty');
      this.readAndApplyAllFilters();
    }, TableFilter.FILTER_DELAY_MS);
  }
}

/**
 * Converts a mojo time to a JS time.
 * @param {!mojoBase.mojom.Time} mojoTime
 * @return {!Date}
 */
function convertMojoTimeToJS(mojoTime: Time) {
  // The JS Date() is based off of the number of milliseconds since the
  // UNIX epoch (1970-01-01 00::00:00 UTC), while |internalValue| of the
  // base::Time (represented in mojom.Time) represents the number of
  // microseconds since the Windows FILETIME epoch (1601-01-01 00:00:00 UTC).
  // This computes the final JS time by computing the epoch delta and the
  // conversion from microseconds to milliseconds.
  const windowsEpoch = Date.UTC(1601, 0, 1, 0, 0, 0, 0);
  const unixEpoch = Date.UTC(1970, 0, 1, 0, 0, 0, 0);
  // |epochDeltaInMs| equals to base::Time::kTimeTToMicrosecondsOffset.
  const epochDeltaInMs = unixEpoch - windowsEpoch;
  const timeInMs = Number(mojoTime.internalValue) / 1000;

  return new Date(timeInMs - epochDeltaInMs);
}

/**
 * Creates the chromium source URL from sourceLocation.
 * @param sourceFile
 * @param sourceLine
 * @param targetElement The element to which source link should be created.
 */
function createChromiumSourceLink(
    sourceFile: string, sourceLine: number, targetElement: Element) {
  // Valid source file starts with ../../
  if (!sourceFile.startsWith('../../')) {
    targetElement.textContent = `${sourceFile}(${sourceLine})`;
    return;
  }
  const fileName = sourceFile.slice(sourceFile.lastIndexOf('/') + 1);
  if (fileName.length == 0) {
    targetElement.textContent = `${sourceFile}(${sourceLine})`;
    return;
  }
  const anchor = document.createElement('a');
  anchor.appendChild(document.createTextNode(`${fileName}(${sourceLine}`));
  anchor.href = `https://source.chromium.org/chromium/chromium/src/+/main:${
      sourceFile.slice(6)};l=${sourceLine}`;
  targetElement.appendChild(anchor);
}

/**
 * Maps the logSource to a human readable string representation.
 * Must be kept in sync with the |LogSource| enum in
 * //components/optimization_guide/core/optimization_guide_common.mojom.
 * @param logSource
 * @returns string
 */
function getLogSource(logSource: number) {
  if (logSource == 0) {
    return 'SERVICE_AND_SETTINGS';
  }
  if (logSource == 1) {
    return 'HINTS';
  }
  if (logSource == 2) {
    return 'MODEL_MANAGEMENT';
  }
  if (logSource == 3) {
    return 'PAGE_CONTENT_ANNOTATIONS';
  }
  if (logSource == 4) {
    return 'HINTS_NOTIFICATIONS';
  }
  if (logSource == 5) {
    return 'TEXT_CLASSIFIER';
  }
  if (logSource == 6) {
    return 'MODEL_EXECUTION';
  }
  if (logSource == 7) {
    return 'NTP_MODULE';
  }
  return logSource.toString();
}

/**
 * The callback to button#log-messages-dump to save the logs to a file.
 */
function onLogMessagesDump() {
  const data = JSON.stringify(logMessages);
  const blob = new Blob([data], {'type': 'text/json'});
  const url = URL.createObjectURL(blob);
  const filename = 'optimization_guide_internals_logs_dump.json';

  const a = document.createElement('a');
  a.setAttribute('href', url);
  a.setAttribute('download', filename);

  const event = document.createEvent('MouseEvent');
  event.initMouseEvent(
      'click', true, true, window, 0, 0, 0, 0, 0, false, false, false, false, 0,
      null);
  a.dispatchEvent(event);
}

async function onModelsPageOpen() {
  const downloadedModelsContainer =
      getRequiredElement<HTMLTableElement>('downloaded-models-container');
  try {
    const response: {downloadedModelsInfo: DownloadedModelInfo[]} =
        await PageHandlerFactory.getRemote().requestDownloadedModelsInfo();
    const downloadedModelsInfo = response.downloadedModelsInfo;
    for (const {optimizationTarget, version, filePath} of
             downloadedModelsInfo) {
      const versionStr = version.toString();
      const existingModel = $<HTMLTableRowElement>(optimizationTarget);
      if (existingModel) {
        existingModel.querySelector('.downloaded-models-version')!.textContent =
            versionStr;
        existingModel.querySelector(
                         '.downloaded-models-file-path')!.textContent =
            filePath;
      } else {
        const downloadedModel = downloadedModelsContainer.insertRow();
        downloadedModel.id = optimizationTarget;
        appendTD(
            downloadedModel, optimizationTarget,
            'downloaded-models-optimization-target');
        appendTD(downloadedModel, versionStr, 'downloaded-models-version');
        appendTD(downloadedModel, filePath, 'downloaded-models-file-path');
      }
    }
  } catch (err) {
    throw new Error(
        `Error resolving promise from requestDownloadedModelsInfo, ${err}`);
  }
}

async function onClientIDsPageOpen() {
  const loggedClientIdsContainer =
      getRequiredElement<HTMLTableElement>('logged-client-ids-container');
  try {
    const response: {loggedClientIds: LoggedClientIds[]} =
        await PageHandlerFactory.getRemote()
            .requestLoggedModelQualityClientIds();
    const loggedClientIds = response.loggedClientIds;
    for (const {clientId} of loggedClientIds) {
      const clientIdStr = clientId.toString();
      const loggedClients = loggedClientIdsContainer.insertRow();
      appendTD(loggedClients, clientIdStr, 'logged-client-ids');
    }
  } catch (err) {
    throw new Error(
        `Error resolving promise from requestLoggedClientIds, ${err}`);
  }
}

/**
 * Appends a new TD element to the specified |parent| element, and returns the
 * newly created element.
 *
 * @param {HTMLTableRowElement} parent The element to which a new TD element is
 *     appended.
 * @param {string} textContent The inner HTML of the element.
 * @param {string} className The class name of the element.
 */
function appendTD(
    parent: HTMLTableRowElement, textContent: string, className: string) {
  const td = parent.insertCell();
  td.textContent = textContent;
  td.className = className;
  parent.appendChild(td);
  return td;
}

function getProxy(): OptimizationGuideInternalsBrowserProxy {
  return OptimizationGuideInternalsBrowserProxy.getInstance();
}


function initialize() {
  const tabbox = document.querySelector('cr-tab-box');
  assert(tabbox);
  tabbox.hidden = false;

  const logMessageContainer =
      getRequiredElement<HTMLTableElement>('log-message-container');

  const tabFilter = new TableFilter(
      logMessageContainer,
      getRequiredElement<HTMLInputElement>('log-message-include'),
      getRequiredElement<HTMLInputElement>('log-message-exclude'),
      getRequiredElement<HTMLSpanElement>('log-message-filter-stats'));

  getRequiredElement('log-messages-dump')
      .addEventListener('click', onLogMessagesDump);

  getProxy().getCallbackRouter().onLogMessageAdded.addListener(
      (eventTime: Time, logSource: number, sourceFile: string,
       sourceLine: number, message: string) => {
        const eventTimeStr = convertMojoTimeToJS(eventTime).toISOString();
        const logSourceStr = getLogSource(logSource);
        logMessages.push({
          eventTime: eventTimeStr,
          logSource: logSourceStr,
          sourceLocation: `${sourceFile}:${sourceLine}`,
          message,
        });
        const logMessage = logMessageContainer.insertRow();
        logMessage.innerHTML =
            window.trustedTypes ? window.trustedTypes.emptyHTML : '';
        appendTD(logMessage, eventTimeStr, 'event-logs-time');
        appendTD(logMessage, logSourceStr, 'event-logs-log-source');
        createChromiumSourceLink(
            sourceFile, sourceLine,
            appendTD(logMessage, '', 'event-logs-source-location'));
        appendTD(logMessage, message, 'event-logs-message');
        tabFilter.filterNewRow(logMessage);
      });

  const tabpanelNodeList = document.querySelectorAll('div[slot=\'panel\']');
  const tabpanels = Array.prototype.slice.call(tabpanelNodeList, 0);
  const tabpanelIds = tabpanels.map(function(tab) {
    return tab.id;
  });

  tabbox.addEventListener('selected-index-change', e => {
    const tabpanel = tabpanels[(e as CustomEvent).detail];
    const hash = tabpanel.id.match(/(?:^tabpanel-)(.+)/)[1];
    window.location.hash = hash;
  });

  const activateTabByHash = function() {
    let hash = window.location.hash;

    // Remove the first character '#'.
    hash = hash.substring(1);

    const id = 'tabpanel-' + hash;
    const index = tabpanelIds.indexOf(id);
    if (index === -1) {
      return;
    }
    tabbox.setAttribute('selected-index', `${index}`);

    if (hash === 'models') {
      onModelsPageOpen();
    } else if (hash === 'client-ids') {
      onClientIDsPageOpen();
    }
  };

  window.onhashchange = activateTabByHash;
  activateTabByHash();
}

document.addEventListener('DOMContentLoaded', initialize);
