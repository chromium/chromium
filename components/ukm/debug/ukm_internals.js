// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   name: string,
 *   value: !Array<number>
 * }}
 */
let Metric;

/**
 * @typedef {{
 *   name: string,
 *   metrics: !Array<!Metric>
 * }}
 */
let UkmEntry;

/**
 * @typedef {{
 *   url: string,
 *   id: !Array<number>,
 *   entries: !Array<UkmEntry>,
 * }}
 */
let UkmDataSource;

/**
 * The Ukm data sent from the browser.
 * @typedef {{
 *   state: boolean,
 *   client_id: !Array<number>,
 *   session_id: string,
 *   sources: !Array<!UkmDataSource>,
 *   is_sampling_enabled: boolean,
 * }}
 */
let UkmData;

/**
 * Stores source id and number of entries shown. If there is a new source id
 * or there are new entries in Ukm recorder, then all the entries for
 * the new source ID will be displayed.
 * @type{Map<string, number>}
 */
const ClearedSources = new Map();

/**
 * Cached sources to persist beyond the log cut. This will ensure that the data
 * on the page don't disappear if there is a log cut. The caching will
 * start when the page is loaded and when the data is refreshed.
 * Stored data is sourceid -> UkmDataSource with array of distinct entries.
 * @type{Map<string, !UkmDataSource>}
 */
const CachedSources = new Map();

/**
 * Text for empty url.
 * @type {string}
 */
const URL_EMPTY = 'missing';

/**
 * Converts a pair of JS 32 bin number to 64 bit hex string. This is used to
 * pass 64 bit numbers from UKM like client id and 64 bit metrics to
 * the javascript.
 * @param {!Array<number>} num A pair of javascript signed int.
 * @return {string} unsigned int64 as hex number or a decimal number if the
 *     value is smaller than 32bit.
 */
function as64Bit(num) {
  if (num.length != 2) {
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
 * @param {!HTMLCollection<!Element>} collection Collection of Elements.
 */
function setDisplayStyle(collection, display_value) {
  for (const el of collection) {
    el.style.display = display_value;
  }
}

/**
 * Remove all the child elements.
 * @param {!Element} parent Parent element whose children will get removed.
 */
function removeChildren(parent) {
  while (parent.firstChild) {
    parent.removeChild(parent.firstChild);
  }
}

/**
 * Create card for URL.
 * @param {!Array<!UkmDataSource>} sourcesForUrl Sources that are for same URL.
 * @param {string} url URL or Source id as hex string if the URL is missing.
 * @param {!Element} sourcesDiv Sources div where this card will be added to.
 * @param {!Map<string, ?string>} displayState Map from source id to value
 *     of display property of the entries div.
 */
function createUrlCard(sourcesForUrl, url, sourcesDiv, displayState) {
  const sourceDiv = createElementWithClassName('div', 'url_card');
  sourcesDiv.appendChild(sourceDiv);
  if (!sourcesForUrl || sourcesForUrl.length === 0) {
    return;
  }
  for (const source of sourcesForUrl) {
    // This div allows hiding of the metrics per URL.
    const sourceContainer = /** @type {!Element} */ (createElementWithClassName(
        'div', 'source_container'));
    sourceDiv.appendChild(sourceContainer);
    createUrlHeader(source.url, source.id, sourceContainer);
    createSourceCard(
        source, sourceContainer, displayState.get(as64Bit(source.id)));
  }
}

/**
 * Create header containing URL and source ID data.
 * @param {?string} url URL.
 * @param {!Array<number>} id SourceId as hex.
 * @param {!Element} sourceDiv Div under which header will get added.
 */
function createUrlHeader(url, id, sourceDiv) {
  const headerElement = createElementWithClassName('div', 'collapsible_header');
  sourceDiv.appendChild(headerElement);
  const urlElement = createElementWithClassName('span', 'url');
  urlElement.innerText = url ? url : URL_EMPTY;
  headerElement.appendChild(urlElement);
  const idElement = createElementWithClassName('span', 'sourceid');
  idElement.innerText = as64Bit(id);
  headerElement.appendChild(idElement);
  // Make the click on header toggle entries div.
  headerElement.addEventListener('click', () => {
    const content = headerElement.nextElementSibling;
    if (content.style.display === 'block') {
      content.style.display = 'none';
    } else {
      content.style.display = 'block';
    }
  });
}

/**
 * Create a card with UKM Source data.
 * @param {!UkmDataSource} source UKM source data.
 * @param {!Element} sourceDiv Source div where this card will be added to.
 * @param {?string} displayState If display style of this source id is modified
 *     then the state of the display style.
 */
function createSourceCard(source, sourceDiv, displayState) {
  const metricElement =
      /** @type {!Element} */ (createElementWithClassName('div', 'entries'));
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
 * @param {!UkmEntry} entry A Ukm metrics Entry.
 * @param {!Element} sourceDiv Element whose children will be the entries.
 */
function createEntryTable(entry, sourceDiv) {
  // Add first column to the table.
  const entryTable = createElementWithClassName('table', 'entry_table');
  entryTable.setAttribute('value', entry.name);
  sourceDiv.appendChild(entryTable);
  const firstRow = document.createElement('tr');
  entryTable.appendChild(firstRow);
  const entryName = createElementWithClassName('td', 'entry_name');
  entryName.setAttribute('rowspan', 0);
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
 * @param {!Array<!UkmDataSource>} sources List of UKM data for a source .
 * @return {!Map<string, !Array<!UkmDataSource>>} Mapping in the sorted
 *     order of URL from URL to list of sources for the URL.
 */
function urlToSourcesMapping(sources) {
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
  return new Map(Array.from(unsorted).sort(
      (s1,s2) => s1[0].localeCompare(s2[0])));
}


/**
 * Adds a button to Expand/Collapse all URLs.
 */
function addExpandToggleButton() {
  const toggleExpand = $('toggle_expand');
  toggleExpand.textContent = 'Expand';
  toggleExpand.addEventListener('click', () => {
    if (toggleExpand.textContent == 'Expand') {
      toggleExpand.textContent = 'Collapse';
      setDisplayStyle(document.getElementsByClassName('entries'), 'block');
    } else {
      toggleExpand.textContent = 'Expand';
      setDisplayStyle(document.getElementsByClassName('entries'), 'none');
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
    cr.sendWithPromise('requestUkmData').then((/** @type {UkmData} */ data) => {
      updateUkmCache(data);
      for (const s of CachedSources.values()) {
        ClearedSources.set(as64Bit(s.id), s.entries.length);
      }
    });
    $('toggle_expand').textContent = 'Expand';
    updateUkmData();
  });
}

/**
 * Populate thread ids from the high bit of source id in sources.
 * @param {!Array<!UkmDataSource>} sources Array of UKM source.
 */
function populateThreadIds(sources) {
  const threadIdSelect = $('thread_ids');
  const currentOptions =
      new Set(Array.from(threadIdSelect.options).map(o => o.value));
  // The first 32 bit of the ID is the recorder ID, convert it to a positive
  // bit patterns and then to hex. Ids that were not seen earlier will get
  // added to the end of the option list.
  const newIds = new Set(sources.map(e => (e.id[0] >>> 0).toString(16)));
  const options = ['All', ...Array.from(newIds).sort()];

  for (const id of options) {
    if (!currentOptions.has(id)) {
      const option = document.createElement("option");
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
 * @param {UkmEntry} entry UKM entry to be stringified.
 * @return {string} Normalized string representation of the entry.
 */
function normalizeToString(entry) {
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
 * @param {UkmData} data New UKM data to add to cache.
 */
function updateUkmCache(data) {
  for (const source of data.sources) {
    const key = as64Bit(source.id);
    if (!CachedSources.has(key)) {
      const mergedSource = {id: source.id, entries: source.entries};
      if (source.url) {
        mergedSource.url = source.url;
      }
      CachedSources.set(key, mergedSource);
    } else {
      // Merge distinct entries from the source.
      const existingEntries = new Set(CachedSources.get(key).entries.map(
          cachedEntry => normalizeToString(cachedEntry)));
      for (const sourceEntry of source.entries) {
        if (!existingEntries.has(normalizeToString(sourceEntry))) {
          CachedSources.get(key).entries.push(sourceEntry);
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
  cr.sendWithPromise('requestUkmData').then((/** @type {UkmData} */ data) => {
    updateUkmCache(data);
    if ($('include_cache').checked) {
      data.sources = [...CachedSources.values()];
    }
    $('state').innerText = data.state? 'ENABLED' : 'DISABLED';
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
      currentDisplayState.set(el.querySelector('.sourceid').textContent,
                              el.querySelector('.entries').style.display);
    }
    const urlToSources = urlToSourcesMapping(
        filterSourcesUsingFormOptions(data.sources));
    for (const url of urlToSources.keys()) {
      const sourcesForUrl = urlToSources.get(url);
      createUrlCard(sourcesForUrl, url, sourcesDiv, currentDisplayState);
    }
    populateThreadIds(data.sources);
  });
}

/**
 * Filter sources that have been recorded previously. If it sees a source id
 * where number of entries has decreased then it will add a warning.
 * @param {!Array<!UkmDataSource>} sources All the sources currently in
 *   UKM recorder.
 * @return {!Array<!UkmDataSource>} Sources which are new or have a new entry
 *   logged for them.
 */
function filterSourcesUsingFormOptions(sources) {
  // Filter sources based on if they have been cleared.
  const newSources = sources.filter(source => (
      // Keep sources if it is newly generated since clearing earlier.
      !ClearedSources.has(as64Bit(source.id)) ||
      // Keep sources if it has increased entities since clearing earlier.
      (source.entries.length > ClearedSources.get(as64Bit(source.id)))
  ));

  // Applies the filter from Metrics selector.
  const newSourcesWithEntriesCleared = newSources.map(source => {
    const metricsFilterValue = $('metrics_select').value;
    if (metricsFilterValue) {
      const metricsRe = new RegExp(metricsFilterValue);
      source.entries = source.entries.filter(e => metricsRe.test(e.name));
    }
    return source;
  });

  // Filter sources based on the status of check-boxes.
  const filteredSources = newSourcesWithEntriesCleared.filter(source => (
      (!$('hide_no_url').checked || source.url) &&
      (!$('hide_no_metrics').checked || source.entries.length)
  ));

  // Filter sources based on thread id (High bits of UKM Recorder ID).
  const threadsFilteredSource = filteredSources.filter(source => {
    // Get current selection for thread id. It is either -
    // "All" for no restriction.
    // "0" for the default thread. This is the thread that record f.e PageLoad
    // <lowercase hex string for first 32 bit of source id> for other threads.
    //     If a UKM is recorded with a custom source id or in renderer, it will
    //     have a unique value for this shared by all metrics that use the
    //     same thread.
    const selectedOption =
        $('thread_ids').options[$('thread_ids').selectedIndex];
    // Return true if either of the following is true -
    // No option is selected or selected option is "All" or the hexadecimal
    // representation of source id is matching.
    return !selectedOption || (selectedOption.value === 'All') ||
        ((source.id[0] >>> 0).toString(16) === selectedOption.value);
  });

  // Filter URLs based on URL selector input.
  return threadsFilteredSource.filter(source => {
    const urlFilterValue = $('url_select').value;
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
