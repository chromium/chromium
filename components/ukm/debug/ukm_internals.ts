// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="is_ios">
import 'chrome://resources/js/ios/web_ui.js';

// </if>

import {assert} from 'chrome://resources/js/assert.js';
import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

interface Metric {
  name: string;
  value: [number, number];
}

interface UkmEvent {
  name: string;
  metrics: Metric[];
}

interface UkmSource {
  id: [number, number];
  type: string;
  events: UkmEvent[];
  url?: string;
}

/**
 * UKM data in the browser's local memory from UkmDebugDataExtractor.
 */
interface UkmSession {
  state: boolean;
  msbb_state: boolean;
  extension_state: boolean;
  app_state: boolean;
  client_id: number[];
  session_id: string;
  sources: UkmSource[];
  is_sampling_enabled: boolean;
}

/**
 * Stores source ID and number of events shown. If there is a new source ID
 * or there are new events in UKM recorder, then all the events for
 * the new source ID will be displayed.
 */
const clearedSources: Map<string, number> = new Map();

/**
 * Cached sources to persist beyond the log cut. This will ensure that the data
 * on the page don't disappear if there is a log cut. The caching will
 * start when the page is loaded and when the data is refreshed.
 * Stored data is sourceid -> UkmSource with array of distinct entries.
 */
const cachedSources: Map<string, UkmSource> = new Map();

/**
 * Text for empty URL.
 */
const URL_EMPTY: string = '(missing)';

const EXPAND_ALL_BUTTON_TEXT: string = 'Expand All';
const COLLAPSE_ALL_BUTTON_TEXT: string = 'Collapse All';

/**
 * Converts a pair of JS 32 bin number to 64 bit hex string. This is used to
 * pass 64 bit numbers from UKM like client id and 64 bit metrics to
 * the javascript.
 * @param num A pair of javascript signed int.
 * @return unsigned int64 as hex number, or a decimal number if the
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
 * Removes all the child elements.
 * @param parent Parent element whose children will be removed.
 */
function removeChildren(parent: Element) {
  while (parent.firstChild) {
    parent.removeChild(parent.firstChild);
  }
}

/**
 * Creates rows in the table body to represent Sources having the same URL
 * value.
 * @param sourcesForUrl Sources having the same URL.
 * @param sourcesTable The <tbody> element representing all Sources to which
 *     Source rows will be added.
 * @param displayStates Map from source ID to value of display property of the
 *     event-metrics tables.
 */
function createSourceRowsForTheSameUrl(
    sourcesForUrl: UkmSource[], sourcesTable: Element,
    displayStates: Map<string, string>) {
  if (!sourcesForUrl || sourcesForUrl.length === 0) {
    return;
  }
  for (const source of sourcesForUrl) {
    const sourceHtmlRow = document.createElement('tr');
    sourceHtmlRow.classList.add('source_container');

    sourcesTable.appendChild(sourceHtmlRow);
    const urlElement = populateSourceHtmlRow(source, sourceHtmlRow);
    const displayState = displayStates.get(as64Bit(source.id));
    createEventMetricTablesForSource(source, urlElement, displayState);
  }
  // Add a thin horizontal line at the bottom, which visually separates this
  // group of Sources with the same URL value from the next group.
  sourcesTable.lastElementChild!.classList.add('thin-border-bottom');
}


/**
 * Populates a table row with the given Source data.
 * @param sourceData data pertaining to one Source.
 * @param sourceHtmlRow The HTML element whose content will be populated.
 * @return The HTML Element representing the URL value to which event-metrics
 *     tables can be appended.
 */
function populateSourceHtmlRow(
    sourceData: UkmSource, sourceHtmlRow: Element): Element {
  const urlElement = document.createElement('td');
  urlElement.classList.add('url');
  urlElement.innerText = sourceData.url || URL_EMPTY;
  const idElement = document.createElement('td');
  idElement.classList.add('sourceid');
  idElement.innerText = as64Bit(sourceData.id);
  const typeElement = document.createElement('td');
  typeElement.classList.add('sourcetype');
  typeElement.innerText = sourceData.type;

  sourceHtmlRow.appendChild(urlElement);
  sourceHtmlRow.appendChild(idElement);
  sourceHtmlRow.appendChild(typeElement);

  // Clicking on the URL of this Source toggles the display state of its
  // event-metrics tables.
  urlElement.addEventListener('click', () => {
    const eventsTables = urlElement.lastChild as HTMLElement;
    eventsTables.style.display =
        eventsTables.style.display === 'block' ? 'none' : 'block';
  });

  return urlElement;
}


/**
 * Adds event-metrics tables of a Source. Clicking on the URL of the Source
 * toggles their display on or off.
 * @param sourceData Data for one Source.
 * @param urlElement The HTML element showing the URL of the source, to which
 *     the event-metrics tables will be added.
 * @param displayState Display style of the event-metrics table for this Source.
 */
function createEventMetricTablesForSource(
    sourceData: UkmSource, urlElement: Element,
    displayState: string|undefined) {
  const eventMetricsElement = document.createElement('div');
  eventMetricsElement.classList.add('events');
  urlElement.appendChild(eventMetricsElement);

  // Apply the display state if any, base on whether the user has clicked this
  // Source row.
  if (displayState) {
    eventMetricsElement.style.display = displayState;
  } else {
    // Apply the display state base on the "Expand/Collapse All" button state.
    const expandedAll = getRequiredElement('toggle_expand').textContent ===
        COLLAPSE_ALL_BUTTON_TEXT;
    eventMetricsElement.style.display = expandedAll ? 'block' : 'none';
  }

  if (sourceData.events.length === 0) {
    eventMetricsElement.textContent = '(no events)';
    return;
  }

  const sortedEvents =
      sourceData.events.sort((e1, e2) => e1.name.localeCompare(e2.name));

  for (const event of sortedEvents) {
    createEventMetricsTable(event, eventMetricsElement);
  }
}


/**
 * Creates a table representing metrics associated to one UKM Event.
 * @param event A UKM Event.
 * @param urlElement The HTML element showing the URL of the source, to which
 *     the event-metrics table will be appended.
 */
function createEventMetricsTable(event: UkmEvent, urlElement: Element) {
  // Add first column to the table.
  const eventTable = document.createElement('table');
  eventTable.classList.add('event-table');
  eventTable.setAttribute('value', event.name);
  urlElement.appendChild(eventTable);

  const firstRow = document.createElement('tr');
  eventTable.appendChild(firstRow);
  const eventName = document.createElement('td');
  eventName.classList.add('event-name');
  eventName.setAttribute('rowspan', '0');
  eventName.textContent = event.name;
  firstRow.appendChild(eventName);

  // Sort the metrics by name, descending.
  const sortedMetrics =
      event.metrics.sort((m1, m2) => m1.name.localeCompare(m2.name));

  // Add metrics rows.
  for (const metric of sortedMetrics) {
    const nextRow = document.createElement('tr');
    const metricName = document.createElement('td');
    metricName.classList.add('metric-name');
    metricName.textContent = metric.name;
    nextRow.appendChild(metricName);

    const metricValue = document.createElement('td');
    metricValue.classList.add('metric-value');
    metricValue.textContent = as64Bit(metric.value);
    nextRow.appendChild(metricValue);

    eventTable.appendChild(nextRow);
  }
}

/**
 * Collect all sources for a particular URL together. It will also sort the
 * URLs alphabetically.
 * If the URL field is missing, the source ID will be used as the
 * URL for the purpose of grouping and sorting.
 * @param sources List of UKM data for a source .
 * @return Mapping in the sorted order of URL from URL to list of sources for
 *     the URL.
 */
function urlToSourcesMapping(sources: UkmSource[]): Map<string, UkmSource[]> {
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
 * Updates the button text for expanding or collapsing all Source rows.
 */
function addExpandAllToggleButton() {
  const toggleExpand = getRequiredElement('toggle_expand');
  toggleExpand.textContent = EXPAND_ALL_BUTTON_TEXT;
  toggleExpand.addEventListener('click', () => {
    if (toggleExpand.textContent === EXPAND_ALL_BUTTON_TEXT) {
      toggleExpand.textContent = COLLAPSE_ALL_BUTTON_TEXT;
      setDisplayStyle(
          document.body.querySelectorAll<HTMLElement>('.events'), 'block');
    } else {
      toggleExpand.textContent = EXPAND_ALL_BUTTON_TEXT;
      setDisplayStyle(
          document.body.querySelectorAll<HTMLElement>('.events'), 'none');
    }
  });
}

/**
 * Updates the button to clear all the existing URLs. Note that the hiding is
 * done in the UI only. So refreshing the page will show all the UKM again.
 * To get the new UKMs after hitting Clear click the refresh button.
 */
function addClearButton() {
  const clearButton = getRequiredElement('clear');
  clearButton.addEventListener('click', () => {
    // Note it won't be able to clear if UKM logs got cut during this call.
    sendWithPromise('requestUkmData').then((/** @type {UkmSession} */ data) => {
      updateUkmCache(data);
      for (const source of cachedSources.values()) {
        clearedSources.set(as64Bit(source.id), source.events.length);
      }
    });
    getRequiredElement('toggle_expand').textContent = EXPAND_ALL_BUTTON_TEXT;
    updateUkmData();
  });
}

/**
 * Populate thread ids from the high bit of Source ID in |sources|.
 * @param sources Array of Sources.
 */
function populateThreadIds(sources: UkmSource[]) {
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
 * Get the string representation of a UKM event. The array of metrics are sorted
 * by name to ensure that two events containing the same metrics and values in
 * different orders have identical string representation to avoid cache
 * duplication.
 * @param event UKM event to be stringified.
 * @return Normalized string representation of the event.
 */
function normalizeToString(event: UkmEvent): string {
  event.metrics.sort((m1, m2) => m1.name.localeCompare(m2.name));
  return JSON.stringify(event);
}

/**
 * This function tries to preserve UKM logs around UKM log uploads. There is
 * no way of knowing if duplicate events for a log are actually produced
 * again after the log cut or if they older records since we don't maintain
 * timestamp with events. So only distinct events will be recorded in the
 * cache. i.e. if two events have exactly the same set of metrics then one
 * of the event will not be kept in the cache.
 * @param data New UKM data to add to cache.
 */
function updateUkmCache(data: UkmSession) {
  for (const source of data.sources) {
    const key = as64Bit(source.id);
    if (!cachedSources.has(key)) {
      const mergedSource:
          UkmSource = {id: source.id, type: source.type, events: source.events};
      if (source.url) {
        mergedSource.url = source.url;
      }
      cachedSources.set(key, mergedSource);
    } else {
      // Merge distinct events from the source.
      const existingEvents = new Set(cachedSources.get(key)!.events.map(
          event => normalizeToString(event)));
      for (const event of source.events) {
        if (!existingEvents.has(normalizeToString(event))) {
          cachedSources.get(key)!.events.push(event);
        }
      }
    }
  }
}

/**
 * Fetches data from the UKM service and updates the DOM to display it as a
 * table.
 */
function updateUkmData() {
  sendWithPromise('requestUkmData').then((/** @type {UkmSession} */ data) => {
    updateUkmCache(data);
    if (document.body.querySelector<HTMLInputElement>(
                         '#include_cache')!.checked) {
      data.sources = [...cachedSources.values()];
    }
    getRequiredElement('state').innerText = data.state ? 'ENABLED' : 'DISABLED';
    getRequiredElement('msbb_state').innerText =
        data.msbb_state ? 'ENABLED' : 'DISABLED';
    getRequiredElement('extension_state').innerText =
        data.extension_state ? 'ENABLED' : 'DISABLED';
    getRequiredElement('app_state').innerText =
        data.app_state ? 'ENABLED' : 'DISABLED';
    getRequiredElement('clientid').innerText = '0x' + data.client_id;
    getRequiredElement('sessionid').innerText = data.session_id;
    getRequiredElement('is_sampling_enabled').innerText =
        data.is_sampling_enabled;

    const sourcesTable = getRequiredElement('sources');
    removeChildren(sourcesTable);

    // Setup the Source table header.
    const tableHead = document.createElement('thead');
    const headerRow = document.createElement('tr');

    const urlTitleElement = document.createElement('td');
    urlTitleElement.classList.add('url');
    urlTitleElement.textContent = 'URL';

    const sourceIdTitleElement = document.createElement('td');
    sourceIdTitleElement.classList.add('sourceid');
    sourceIdTitleElement.textContent = 'Source ID';

    const sourceTypeTitleElement = document.createElement('td');
    sourceTypeTitleElement.classList.add('sourcetype');
    sourceTypeTitleElement.textContent = 'Source Type';

    headerRow.appendChild(urlTitleElement);
    headerRow.appendChild(sourceIdTitleElement);
    headerRow.appendChild(sourceTypeTitleElement);
    tableHead.appendChild(headerRow);
    sourcesTable.appendChild(tableHead);

    const tableBody = document.createElement('tbody');
    tableBody.classList.add('url_card');
    sourcesTable.appendChild(tableBody);

    // Setup the display state map, which captures the current display settings,
    // for example, expanded state.
    const currentDisplayStates = new Map();
    for (const el of document.getElementsByClassName('source_container')) {
      currentDisplayStates.set(
          el.querySelector<HTMLElement>('.sourceid')!.textContent,
          el.querySelector<HTMLElement>('.events')!.style.display);
    }
    const urlToSources =
        urlToSourcesMapping(filterSourcesUsingFormOptions(data.sources));
    for (const url of urlToSources.keys()) {
      const sourcesForUrl = urlToSources.get(url)!;
      createSourceRowsForTheSameUrl(
          sourcesForUrl, tableBody, currentDisplayStates);
    }
    populateThreadIds(data.sources);
  });
}

/**
 * Filters sources that have been recorded previously. If it sees a source ID
 * where number of events has decreased then it will add a warning.
 * @param sources All the sources currently in the UKM recorder.
 * @return Sources which are new or have a new event logged for them.
 */
function filterSourcesUsingFormOptions(sources: UkmSource[]): UkmSource[] {
  // Filter sources based on if they have been cleared.
  const newSources = sources.filter(
      source => (
          // Keep a Source if it is newly created since clearing earlier.
          !clearedSources.has(as64Bit(source.id)) ||
          // Keep a Source if it contains more events since clearing earlier.
          (source.events.length > clearedSources.get(as64Bit(source.id))!)));

  // Apply the event name filtering.
  const newSourcesWithEventsCleared = newSources.map(source => {
    const eventNameFilterValue =
        document.body.querySelector<HTMLInputElement>('#events_select')!.value;
    if (eventNameFilterValue) {
      const filterRe = new RegExp(eventNameFilterValue);
      source.events = source.events.filter(event => filterRe.test(event.name));
    }
    return source;
  });

  // Filter sources based on the status of check-boxes.
  const filteredSources = newSourcesWithEventsCleared.filter(source => {
    const noUrlCheckbox =
        document.body.querySelector<HTMLInputElement>('#hide_no_url');
    assert(noUrlCheckbox);
    const noMetricsCheckbox =
        document.body.querySelector<HTMLInputElement>('#hide_no_events');
    assert(noMetricsCheckbox);

    return (!noUrlCheckbox.checked || source.url) &&
        (!noMetricsCheckbox.checked || source.events.length);
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
  addExpandAllToggleButton();
  addClearButton();
  updateUkmData();
  getRequiredElement('refresh').addEventListener('click', updateUkmData);
  getRequiredElement('hide_no_events').addEventListener('click', updateUkmData);
  getRequiredElement('hide_no_url').addEventListener('click', updateUkmData);
  getRequiredElement('thread_ids').addEventListener('click', updateUkmData);
  getRequiredElement('include_cache').addEventListener('click', updateUkmData);
  getRequiredElement('events_select').addEventListener('keyup', e => {
    if (e.key === 'Enter') {
      updateUkmData();
    }
  });
  getRequiredElement('url_select').addEventListener('keyup', e => {
    if (e.key === 'Enter') {
      updateUkmData();
    }
  });
}

document.addEventListener('DOMContentLoaded', onLoad);

setInterval(updateUkmData, 2 * 60 * 1000);  // Refresh every 2 minutes.
