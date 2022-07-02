// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="is_ios">
import 'chrome://resources/js/ios/web_ui.js';

// </if>

import {assert} from 'chrome://resources/js/assert_ts.js';
import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {$, createElementWithClassName} from 'chrome://resources/js/util.m.js';

type Metric = {
  name: string,
  value: [number, number],
};

type UkmEntry = {
  name: string,
  metrics: Metric[],
};

type UkmDataSource = {
  id: [number, number],
  entries: UkmEntry[],
  url?: string,
};

/**
 * The Ukm data sent from the browser.
 */
type UkmData = {
  state: boolean,
  client_id: number[],
  session_id: string,
  sources: UkmDataSource[],
  is_sampling_enabled: boolean,
};

/**
 * Stores source id and number of entries shown. If there is a new source id
 * or there are new entries in Ukm recorder, then all the entries for
 * the new source ID will be displayed.
 */
const clearedSources: Map<string, number> = new Map();

/**
 * Cached sources to persist beyond the log cut. This will ensure that the data
 * on the page don't disappear if there is a log cut. The caching will
 * start when the page is loaded and when the data is refreshed.
 * Stored data is sourceid -> UkmDataSource with array of distinct entries.
 */
const cachedSources: Map<string, UkmDataSource> = new Map();

/**
 * Text for empty url.
 */
const URL_EMPTY: string = 'missing';

/**
 * Converts a pair of JS 32 bin number to 64 bit hex string. This is used to
 * pass 64 bit numbers from UKM like client id and 64 bit metrics to
 * the javascript.
 * @param num A pair of javascript signed int.
 * @return unsigned int64 as hex number or a decimal number if the
 *     value is smaller than 32bit.
 */
function as64Bit(num: [number, number]): string {
  if (num.length !== 2) {
    return '0';
  }
  if (!num[0]) {
    return num[1].toString();  // Return the lsb as String.
  } else {
    const hi = (num[0] >>> 0).toString(16).padStart(8, '0');
    const lo = (num[1] >>> 0).toString(16).padStart(8, '0');
    return `0x${hi}${lo}`;
  }
}

/**
 * Sets the display option of all the elements in HtmlCollection to the value
 * passed.
 */
function setDisplayStyle(
    elements: NodeListOf<HTMLElement>, displayValue: string) {
  for (const el of elements) {
    el.style.display = displayValue;
  }
}

/**
 * Remove all the child elements.
 * @param parent Parent element whose children will get removed.
 */
function removeChildren(parent: Element) {
  while (parent.firstChild) {
    parent.removeChild(parent.firstChild);
  }
}

/**
 * Create card for URL.
 * @param sourcesForUrl Sources that are for same URL.
 * @param sourcesDiv Sources div where this card will be added to.
 * @param displayState Map from source id to value of display property of the
 *     entries div.
 */
function createUrlCard(
    sourcesForUrl: UkmDataSource[], sourcesDiv: Element,
    displayState: Map<string, string>) {
  const sourceDiv = createElementWithClassName('div', 'url_card');
  sourcesDiv.appendChild(sourceDiv);
  if (!sourcesForUrl || sourcesForUrl.length === 0) {
    return;
  }
  for (const source of sourcesForUrl) {
    // This div allows hiding of the metrics per URL.
    const sourceContainer = /** @type {!Element} */ (
        createElementWithClassName('div', 'source_container'));
    sourceDiv.appendChild(sourceContainer);
    createUrlHeader(source.url, source.id, sourceContainer);
    createSourceCard(
        source, sourceContainer, displayState.get(as64Bit(source.id)));
  }
}

/**
 * Create header containing URL and source ID data.
 * @param id SourceId as hex.
 * @param sourceDiv Div under which header will get added.
 */
function createUrlHeader(
    url: string|undefined, id: [number, number], sourceDiv: Element) {
  const headerElement = createElementWithClassName('div', 'collapsible_header');
  sourceDiv.appendChild(headerElement);
  const urlElement = createElementWithClassName('span', 'url') as HTMLElement;
  urlElement.innerText = url ? url : URL_EMPTY;
  headerElement.appendChild(urlElement);
  const idElement =
      createElementWithClassName('span', 'sourceid') as HTMLElement;
  idElement.innerText = as64Bit(id);
  headerElement.appendChild(idElement);
  // Make the click on header toggle entries div.
  headerElement.addEventListener('click', () => {
    const content = headerElement.nextElementSibling as HTMLElement;
    if (content.style.display === 'block') {
      content.style.display = 'none';
    } else {
      content.style.display = 'block';
    }
  });
}

/**
 * Create a card with UKM Source data.
 * @param source UKM source data.
 * @param sourceDiv Source div where this card will be added to.
 * @param displayState If display style of this source id is modified
 *     then the state of the display style.
 */
function createSourceCard(
    source: UkmDataSource, sourceDiv: Element, displayState: string|undefined) {
  const metricElement =
      createElementWithClassName('div', 'entries') as HTMLElement;
  sourceDiv.appendChild(metricElement);
  const sortedEntry =
      source.entries.sort((x, y) => x.name.localeCompare(y.name));
  for (const entry of sortedEntry) {
    createEntryTable(entry, metricElement);
  }
  if (displayState) {
    metricElement.style.display = displayState;
  } else {
    if ($('toggle_expand').textContent === 'Collapse') {
      metricElement.style.display = 'block';
    } else {
      metricElement.style.display = 'none';
    }
  }
}


/**
 * Create UKM Entry Table.
 * @param entry A Ukm metrics Entry.
 * @param sourceDiv Element whose children will be the entries.
 */
function createEntryTable(entry: UkmEntry, sourceDiv: Element) {
  // Add first column to the table.
  const entryTable = createElementWithClassName('table', 'entry_table');
  entryTable.setAttribute('value', entry.name);
  sourceDiv.appendChild(entryTable);
  const firstRow = document.createElement('tr');
  entryTable.appendChild(firstRow);
  const entryName = createElementWithClassName('td', 'entry_name');
  entryName.setAttribute('rowspan', '0');
  entryName.textContent = entry.name;
  firstRow.appendChild(entryName);

  // Sort the metrics by name, descending.
  const sortedMetrics =
      entry.metrics.sort((x, y) => x.name.localeCompare(y.name));

  // Add metrics columns.
  for (const metric of sortedMetrics) {
    const nextRow = document.createElement('tr');
    const metricName = createElementWithClassName('td', 'metric_name');
    metricName.textContent = metric.name;
    nextRow.appendChild(metricName);
    const metricValue = createElementWithClassName('td', 'metric_value');
    metricValue.textContent = as64Bit(metric.value);
    nextRow.appendChild(metricValue);
    entryTable.appendChild(nextRow);
  }
}

/**
 * Collect all sources for a particular URL together. It will also sort the
 * urls alphabetically.
 * If the URL field is missing, the source ID will be used as the
 * URL for the purpose of grouping and sorting.
 * @param sources List of UKM data for a source .
 * @return Mapping in the sorted order of URL from URL to list of sources for
 *     the URL.
 */
function urlToSourcesMapping(sources: UkmDataSource[]):
    Map<string, UkmDataSource[]> {
  const unsorted = new Map();
  for (const source of sources) {
    const key = source.url ? source.url : as64Bit(source.id);
    if (!unsorted.has(key)) {
      unsorted.set(key, [source]);
    } else {
      unsorted.get(key).push(source);
    }
  }
  // Sort the map by URLs.
  return new Map(
      Array.from(unsorted).sort((s1, s2) => s1[0].localeCompare(s2[0])));
}


/**
 * Adds a button to Expand/Collapse all URLs.
 */
function addExpandToggleButton() {
  const toggleExpand = $('toggle_expand');
  toggleExpand.textContent = 'Expand';
  toggleExpand.addEventListener('click', () => {
    if (toggleExpand.textContent === 'Expand') {
      toggleExpand.textContent = 'Collapse';
      setDisplayStyle(
          document.body.querySelectorAll<HTMLElement>('.entries'), 'block');
    } else {
      toggleExpand.textContent = 'Expand';
      setDisplayStyle(
          document.body.querySelectorAll<HTMLElement>('.entries'), 'none');
    }
  });
}

/**
 * Adds a button to clear all the existing URLs. Note that the hiding is
 * done in the UI only. So refreshing the page will show all the UKM again.
 * To get the new UKMs after hitting Clear click the refresh button.
 */
function addClearButton() {
  const clearButton = $('clear');
  clearButton.addEventListener('click', () => {
    // Note it won't be able to clear if UKM logs got cut during this call.
    sendWithPromise('requestUkmData').then((/** @type {UkmData} */ data) => {
      updateUkmCache(data);
      for (const s of cachedSources.values()) {
        clearedSources.set(as64Bit(s.id), s.entries.length);
      }
    });
    $('toggle_expand').textContent = 'Expand';
    updateUkmData();
  });
}

/**
 * Populate thread ids from the high bit of source id in sources.
 * @param sources Array of UKM source.
 */
function populateThreadIds(sources: UkmDataSource[]) {
  const threadIdSelect =
      document.body.querySelector<HTMLSelectElement>('#thread_ids');
  assert(threadIdSelect);
  const currentOptions =
      new Set(Array.from(threadIdSelect.options).map(o => o.value));
  // The first 32 bit of the ID is the recorder ID, convert it to a positive
  // bit patterns and then to hex. Ids that were not seen earlier will get
  // added to the end of the option list.
  const newIds = new Set(sources.map(e => (e.id[0] >>> 0).toString(16)));
  const options = ['All', ...Array.from(newIds).sort()];

  for (const id of options) {
    if (!currentOptions.has(id)) {
      const option = document.createElement('option');
      option.textContent = id;
      option.setAttribute('value', id);
      threadIdSelect.add(option);
    }
  }
}

/**
 * Get the string representation of a UKM entry. The array of metrics are sorted
 * by name to ensure that two entries containing the same metrics and values in
 * different orders have identical string representation to avoid cache
 * duplication.
 * @param entry UKM entry to be stringified.
 * @return Normalized string representation of the entry.
 */
function normalizeToString(entry: UkmEntry): string {
  entry.metrics.sort((x, y) => x.name.localeCompare(y.name));
  return JSON.stringify(entry);
}

/**
 * This function tries to preserve UKM logs around UKM log uploads. There is
 * no way of knowing if duplicate entries for a log are actually produced
 * again after the log cut or if they older records since we don't maintain
 * timestamp with entries. So only distinct entries will be recorded in the
 * cache. i.e if two entries have exactly the same set of metrics then one
 * of the entry will not be kept in the cache.
 * @param data New UKM data to add to cache.
 */
function updateUkmCache(data: UkmData) {
  for (const source of data.sources) {
    const key = as64Bit(source.id);
    if (!cachedSources.has(key)) {
      const mergedSource:
          UkmDataSource = {id: source.id, entries: source.entries};
      if (source.url) {
        mergedSource.url = source.url;
      }
      cachedSources.set(key, mergedSource);
    } else {
      // Merge distinct entries from the source.
      const existingEntries = new Set(cachedSources.get(key)!.entries.map(
          cachedEntry => normalizeToString(cachedEntry)));
      for (const sourceEntry of source.entries) {
        if (!existingEntries.has(normalizeToString(sourceEntry))) {
          cachedSources.get(key)!.entries.push(sourceEntry);
        }
      }
    }
  }
}

/**
 * Fetches data from the Ukm service and updates the DOM to display it as a
 * list.
 */
function updateUkmData() {
  sendWithPromise('requestUkmData').then((/** @type {UkmData} */ data) => {
    updateUkmCache(data);
    if (document.body.querySelector<HTMLInputElement>(
                         '#include_cache')!.checked) {
      data.sources = [...cachedSources.values()];
    }
    $('state').innerText = data.state ? 'ENABLED' : 'DISABLED';
    $('clientid').innerText = '0x' + data.client_id;
    $('sessionid').innerText = data.session_id;
    $('is_sampling_enabled').innerText = data.is_sampling_enabled;

    const sourcesDiv = /** @type {!Element} */ ($('sources'));
    removeChildren(sourcesDiv);

    // Setup a title for the sources div.
    const urlTitleElement = createElementWithClassName('span', 'url');
    urlTitleElement.textContent = 'URL';
    const sourceIdTitleElement = createElementWithClassName('span', 'sourceid');
    sourceIdTitleElement.textContent = 'Source ID';
    sourcesDiv.appendChild(urlTitleElement);
    sourcesDiv.appendChild(sourceIdTitleElement);

    // Setup the display state map, which captures the current display settings,
    // for example, expanded state.
    const currentDisplayState = new Map();
    for (const el of document.getElementsByClassName('source_container')) {
      currentDisplayState.set(
          el.querySelector<HTMLElement>('.sourceid')!.textContent,
          el.querySelector<HTMLElement>('.entries')!.style.display);
    }
    const urlToSources =
        urlToSourcesMapping(filterSourcesUsingFormOptions(data.sources));
    for (const url of urlToSources.keys()) {
      const sourcesForUrl = urlToSources.get(url)!;
      createUrlCard(sourcesForUrl, sourcesDiv, currentDisplayState);
    }
    populateThreadIds(data.sources);
  });
}

/**
 * Filter sources that have been recorded previously. If it sees a source id
 * where number of entries has decreased then it will add a warning.
 * @param sources All the sources currently in UKM recorder.
 * @return Sources which are new or have a new entry logged for them.
 */
function filterSourcesUsingFormOptions(sources: UkmDataSource[]):
    UkmDataSource[] {
  // Filter sources based on if they have been cleared.
  const newSources = sources.filter(
      source => (
          // Keep sources if it is newly generated since clearing earlier.
          !clearedSources.has(as64Bit(source.id)) ||
          // Keep sources if it has increased entities since clearing earlier.
          (source.entries.length > clearedSources.get(as64Bit(source.id))!)));

  // Applies the filter from Metrics selector.
  const newSourcesWithEntriesCleared = newSources.map(source => {
    const metricsFilterValue =
        document.body.querySelector<HTMLInputElement>('#metrics_select')!.value;
    if (metricsFilterValue) {
      const metricsRe = new RegExp(metricsFilterValue);
      source.entries = source.entries.filter(e => metricsRe.test(e.name));
    }
    return source;
  });

  // Filter sources based on the status of check-boxes.
  const filteredSources = newSourcesWithEntriesCleared.filter(source => {
    const noUrlCheckbox =
        document.body.querySelector<HTMLInputElement>('#hide_no_url');
    assert(noUrlCheckbox);
    const noMetricsCheckbox =
        document.body.querySelector<HTMLInputElement>('#hide_no_metrics');
    assert(noMetricsCheckbox);

    return (!noUrlCheckbox.checked || source.url) &&
        (!noMetricsCheckbox.checked || source.entries.length);
  });

  const threadIds =
      document.body.querySelector<HTMLSelectElement>('#thread_ids');
  assert(threadIds);

  // Filter sources based on thread id (High bits of UKM Recorder ID).
  const threadsFilteredSource = filteredSources.filter(source => {
    // Get current selection for thread id. It is either -
    // "All" for no restriction.
    // "0" for the default thread. This is the thread that record f.e PageLoad
    // <lowercase hex string for first 32 bit of source id> for other threads.
    //     If a UKM is recorded with a custom source id or in renderer, it will
    //     have a unique value for this shared by all metrics that use the
    //     same thread.
    const selectedOption = threadIds.options[threadIds.selectedIndex];
    // Return true if either of the following is true -
    // No option is selected or selected option is "All" or the hexadecimal
    // representation of source id is matching.
    return !selectedOption || (selectedOption.value === 'All') ||
        ((source.id[0] >>> 0).toString(16) === selectedOption.value);
  });

  // Filter URLs based on URL selector input.
  const urlSelect =
      document.body.querySelector<HTMLInputElement>('#url_select');
  assert(urlSelect);
  return threadsFilteredSource.filter(source => {
    const urlFilterValue = urlSelect.value;
    if (urlFilterValue) {
      const urlRe = new RegExp(urlFilterValue);
      // Will also match missing URLs by default.
      return !source.url || urlRe.test(source.url);
    }
    return true;
  });
}

/**
 * DomContentLoaded handler.
 */
function onLoad() {
  addExpandToggleButton();
  addClearButton();
  updateUkmData();
  $('refresh').addEventListener('click', updateUkmData);
  $('hide_no_metrics').addEventListener('click', updateUkmData);
  $('hide_no_url').addEventListener('click', updateUkmData);
  $('thread_ids').addEventListener('click', updateUkmData);
  $('include_cache').addEventListener('click', updateUkmData);
  $('metrics_select').addEventListener('keyup', e => {
    if (e.key === 'Enter') {
      updateUkmData();
    }
  });
  $('url_select').addEventListener('keyup', e => {
    if (e.key === 'Enter') {
      updateUkmData();
    }
  });
}

document.addEventListener('DOMContentLoaded', onLoad);

setInterval(updateUkmData, 120000);  // Refresh every 2 minutes.
