// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('chrome.sync.about_tab', function() {
  // Contains the latest snapshot of sync about info.
  chrome.sync.aboutInfo = {};

  function highlightIfChanged(node, oldVal, newVal) {
    function clearHighlight() {
      this.removeAttribute('highlighted');
    }

    const oldStr = oldVal.toString();
    const newStr = newVal.toString();
    if (oldStr != '' && oldStr != newStr) {
      // Note the addListener function does not end up creating duplicate
      // listeners.  There can be only one listener per event at a time.
      // Reference: https://developer.mozilla.org/en/DOM/element.addEventListener
      node.addEventListener('webkitAnimationEnd', clearHighlight, false);
      node.setAttribute('highlighted', '');
    }
  }

  function refreshAboutInfo(aboutInfo) {
    chrome.sync.aboutInfo = aboutInfo;
    const aboutInfoDiv = $('about-info');
    jstProcess(new JsEvalContext(aboutInfo), aboutInfoDiv);
  }

  function onAboutInfoUpdatedEvent(e) {
    refreshAboutInfo(e.details);
  }

  function onAboutInfoCountersUpdated(e) {
    const details = e.details;

    const modelType = details.modelType;
    const counters = details.counters;

    const typeStatusArray = chrome.sync.aboutInfo.type_status;
    typeStatusArray.forEach(function(row) {
      if (row.name == modelType) {
        // There are three types of counters, only "status" counters have these
        // fields. Keep the old values if updated fields are not present.
        if (counters.numEntriesAndTombstones !== undefined) {
          row.num_entries = counters.numEntriesAndTombstones;
        }
        if (counters.numEntries !== undefined) {
          row.num_live = counters.numEntries;
        }
      }
    });
    jstProcess(
        new JsEvalContext({type_status: typeStatusArray}), $('typeInfo'));
  }

  /**
   * Helper to determine if an element is scrolled to its bottom limit.
   * @param {Element} elem element to check
   * @return {boolean} true if the element is scrolled to the bottom
   */
  function isScrolledToBottom(elem) {
    return elem.scrollHeight - elem.scrollTop == elem.clientHeight;
  }

  /**
   * Helper to scroll an element to its bottom limit.
   * @param {Element} elem element to be scrolled
   */
  function scrollToBottom(elem) {
    elem.scrollTop = elem.scrollHeight - elem.clientHeight;
  }

  /** Container for accumulated sync protocol events. */
  const protocolEvents = [];

  /** We may receive re-delivered events.  Keep a record of ones we've seen. */
  const knownEventTimestamps = {};

  /**
   * Callback for incoming protocol events.
   * @param {Event} e The protocol event.
   */
  function onReceivedProtocolEvent(e) {
    const details = e.details;

    // Return early if we've seen this event before.  Assumes that timestamps
    // are sufficiently high resolution to uniquely identify an event.
    if (knownEventTimestamps.hasOwnProperty(details.time)) {
      return;
    }

    knownEventTimestamps[details.time] = true;
    protocolEvents.push(details);

    const trafficContainer = $('traffic-event-container');

    // Scroll to the bottom if we were already at the bottom.  Otherwise, leave
    // the scrollbar alone.
    const shouldScrollDown = isScrolledToBottom(trafficContainer);

    const context = new JsEvalContext({events: protocolEvents});
    jstProcess(context, trafficContainer);

    if (shouldScrollDown) {
      scrollToBottom(trafficContainer);
    }
  }

  /**
   * Initializes state and callbacks for the protocol event log UI.
   */
  function initProtocolEventLog() {
    const includeSpecificsCheckbox = $('capture-specifics');
    includeSpecificsCheckbox.addEventListener('change', function(event) {
      chrome.sync.setIncludeSpecifics(includeSpecificsCheckbox.checked);
    });

    chrome.sync.events.addEventListener(
        'onProtocolEvent', onReceivedProtocolEvent);

    // Make the prototype jscontent element disappear.
    jstProcess({}, $('traffic-event-container'));

    const triggerRefreshButton = $('trigger-refresh');
    triggerRefreshButton.addEventListener('click', function(event) {
      chrome.sync.triggerRefresh();
    });
  }

  /**
   * Initializes listeners for status dump and import UI.
   */
  function initStatusDumpButton() {
    $('status-data').hidden = true;

    const dumpStatusButton = $('dump-status');
    dumpStatusButton.addEventListener('click', function(event) {
      const aboutInfo = chrome.sync.aboutInfo;
      if (!$('include-ids').checked) {
        aboutInfo.details = chrome.sync.aboutInfo.details.filter(function(el) {
          return !el.is_sensitive;
        });
      }
      let data = '';
      data += new Date().toString() + '\n';
      data += '======\n';
      data += 'Status\n';
      data += '======\n';
      data += JSON.stringify(aboutInfo, null, 2) + '\n';

      $('status-text').value = data;
      $('status-data').hidden = false;
    });

    const importStatusButton = $('import-status');
    importStatusButton.addEventListener('click', function(event) {
      $('status-data').hidden = false;
      if ($('status-text').value.length == 0) {
        $('status-text').value =
            'Paste sync status dump here then click import.';
        return;
      }

      // First remove any characters before the '{'.
      let data = $('status-text').value;
      const firstBrace = data.indexOf('{');
      if (firstBrace < 0) {
        $('status-text').value = 'Invalid sync status dump.';
        return;
      }
      data = data.substr(firstBrace);

      // Remove listeners to prevent sync events from overwriting imported data.
      chrome.sync.events.removeEventListener(
          'onAboutInfoUpdated',
          onAboutInfoUpdatedEvent);

      chrome.sync.events.removeEventListener(
          'onCountersUpdated',
          onAboutInfoCountersUpdated);

      const aboutInfo = JSON.parse(data);
      refreshAboutInfo(aboutInfo);
    });
  }

  /**
   * Toggles the given traffic event entry div's "expanded" state.
   * @param {MouseEvent} e the click event that triggered the toggle.
   */
  function expandListener(e) {
    if (e.target.classList.contains("proto")) {
      // We ignore proto clicks to keep it copyable.
      return;
    }
    let trafficEventDiv = e.target;
    // Click might be on div's child.
    if (trafficEventDiv.nodeName != 'DIV') {
      trafficEventDiv = trafficEventDiv.parentNode;
    }
    trafficEventDiv.classList.toggle('traffic-event-entry-expanded');
  }

  /**
   * Attaches a listener to the given traffic event entry div.
   * @param {HTMLElement} element the element to attach the listener to.
   */
  function addExpandListener(element) {
    element.addEventListener('click', expandListener, false);
  }

  function onLoad() {
    initStatusDumpButton();
    initProtocolEventLog();

    chrome.sync.events.addEventListener(
        'onAboutInfoUpdated',
        onAboutInfoUpdatedEvent);

    chrome.sync.events.addEventListener(
        'onCountersUpdated',
        onAboutInfoCountersUpdated);

    $('request-start').addEventListener('click', function(event) {
      chrome.sync.requestStart();
    });
    $('request-stop-keep-data').addEventListener('click', function(event) {
      chrome.sync.requestStopKeepData();
    });
    $('request-stop-clear-data').addEventListener('click', function(event) {
      chrome.sync.requestStopClearData();
    });

    // Register to receive a stream of event notifications.
    chrome.sync.registerForEvents();

    // Request an about info update event to initialize the page.
    chrome.sync.requestUpdatedAboutInfo();
  }

  return {
    onLoad: onLoad,
    addExpandListener: addExpandListener,
    highlightIfChanged: highlightIfChanged
  };
});

document.addEventListener(
    'DOMContentLoaded', chrome.sync.about_tab.onLoad, false);
