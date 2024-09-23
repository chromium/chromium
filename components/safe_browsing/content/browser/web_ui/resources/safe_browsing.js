/* Copyright 2017 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

import 'chrome://resources/cr_elements/cr_tab_box/cr_tab_box.js';

import {addWebUiListener, sendWithPromise} from 'chrome://resources/js/cr.js';
import {$} from 'chrome://resources/js/util.js';

/**
 * Asks the C++ SafeBrowsingUIHandler to get the lists of Safe Browsing
 * ongoing experiments and preferences.
 * The SafeBrowsingUIHandler should reply to addExperiment() and
 * addPreferences() (below).
 */
function initialize() {
  sendWithPromise('getExperiments', [])
      .then((experiments) => addExperiments(experiments));
  sendWithPromise('getPrefs', []).then((prefs) => addPrefs(prefs));
  sendWithPromise('getPolicies', []).then((policies) => addPolicies(policies));
  sendWithPromise('getCookie', []).then((cookie) => addCookie(cookie));
  sendWithPromise('getSavedPasswords', [])
      .then((passwords) => addSavedPasswords(passwords));
  sendWithPromise('getDatabaseManagerInfo', []).then(function(databaseState) {
    const fullHashCacheState = databaseState.splice(-1, 1);
    addDatabaseManagerInfo(databaseState);
    addFullHashCacheInfo(fullHashCacheState);
  });

  sendWithPromise('getDownloadUrlsChecked', []).then((urlsChecked) => {
    urlsChecked.forEach(function(url_and_result) {
      addDownloadUrlChecked(url_and_result);
    });
  });
  addWebUiListener('download-url-checked-update', function(url_and_result) {
    addDownloadUrlChecked(url_and_result);
  });

  sendWithPromise('getSentClientDownloadRequests', [])
      .then((sentClientDownloadRequests) => {
        sentClientDownloadRequests.forEach(function(cdr) {
          addSentClientDownloadRequestsInfo(cdr);
        });
      });
  addWebUiListener('sent-client-download-requests-update', function(result) {
    addSentClientDownloadRequestsInfo(result);
  });

  sendWithPromise('getReceivedClientDownloadResponses', [])
      .then((receivedClientDownloadResponses) => {
        receivedClientDownloadResponses.forEach(function(cdr) {
          addReceivedClientDownloadResponseInfo(cdr);
        });
      });
  addWebUiListener(
      'received-client-download-responses-update', function(result) {
        addReceivedClientDownloadResponseInfo(result);
      });

  sendWithPromise('getSentClientPhishingRequests', [])
      .then((sentClientPhishingRequests) => {
        sentClientPhishingRequests.forEach(function(cpr) {
          addSentClientPhishingRequestsInfo(cpr);
        });
      });
  addWebUiListener('sent-client-phishing-requests-update', function(result) {
    addSentClientPhishingRequestsInfo(result);
  });

  sendWithPromise('getReceivedClientPhishingResponses', [])
      .then((receivedClientPhishingResponses) => {
        receivedClientPhishingResponses.forEach(function(cpr) {
          addReceivedClientPhishingResponseInfo(cpr);
        });
      });
  addWebUiListener(
      'received-client-phishing-responses-update', function(result) {
        addReceivedClientPhishingResponseInfo(result);
      });

  sendWithPromise('getSentCSBRRs', []).then((sentCSBRRs) => {
    sentCSBRRs.forEach(function(csbrr) {
      addSentCSBRRsInfo(csbrr);
    });
  });
  addWebUiListener('sent-csbrr-update', function(result) {
    addSentCSBRRsInfo(result);
  });

  sendWithPromise('getSentHitReports', []).then((sentHitReports) => {
    sentHitReports.forEach(function(hitReports) {
      addSentHitReportsInfo(hitReports);
    });
  });
  addWebUiListener('sent-hit-report-list', function(result) {
    addSentHitReportsInfo(result);
  });

  sendWithPromise('getPGEvents', []).then((pgEvents) => {
    pgEvents.forEach(function(pgEvent) {
      addPGEvent(pgEvent);
    });
  });
  addWebUiListener('sent-pg-event', function(result) {
    addPGEvent(result);
  });

  sendWithPromise('getSecurityEvents', []).then((securityEvents) => {
    securityEvents.forEach(function(securityEvent) {
      addSecurityEvent(securityEvent);
    });
  });
  addWebUiListener('sent-security-event', function(result) {
    addSecurityEvent(result);
  });

  sendWithPromise('getPGPings', []).then((pgPings) => {
    pgPings.forEach(function(pgPing) {
      addPGPing(pgPing);
    });
  });
  addWebUiListener('pg-pings-update', function(result) {
    addPGPing(result);
  });

  sendWithPromise('getPGResponses', []).then((pgResponses) => {
    pgResponses.forEach(function(pgResponse) {
      addPGResponse(pgResponse);
    });
  });
  addWebUiListener('pg-responses-update', function(result) {
    addPGResponse(result);
  });

  sendWithPromise('getURTLookupPings', []).then((urtLookupPings) => {
    urtLookupPings.forEach(function(urtLookupPing) {
      addURTLookupPing(urtLookupPing);
    });
  });
  addWebUiListener('urt-lookup-pings-update', function(result) {
    addURTLookupPing(result);
  });

  sendWithPromise('getURTLookupResponses', []).then((urtLookupResponses) => {
    urtLookupResponses.forEach(function(urtLookupResponse) {
      addURTLookupResponse(urtLookupResponse);
    });
  });
  addWebUiListener('urt-lookup-responses-update', function(result) {
    addURTLookupResponse(result);
  });

  sendWithPromise('getHPRTLookupPings', []).then((hprtLookupPings) => {
    hprtLookupPings.forEach(function(hprtLookupPing) {
      addHPRTLookupPing(hprtLookupPing);
    });
  });
  addWebUiListener('hprt-lookup-pings-update', function(result) {
    addHPRTLookupPing(result);
  });

  sendWithPromise('getHPRTLookupResponses', []).then((hprtLookupResponses) => {
    hprtLookupResponses.forEach(function(hprtLookupResponse) {
      addHPRTLookupResponse(hprtLookupResponse);
    });
  });
  addWebUiListener('hprt-lookup-responses-update', function(result) {
    addHPRTLookupResponse(result);
  });

  sendWithPromise('getLogMessages', []).then((logMessages) => {
    logMessages.forEach(function(message) {
      addLogMessage(message);
    });
  });
  addWebUiListener('log-messages-update', function(message) {
    addLogMessage(message);
  });

  sendWithPromise('getReportingEvents', []).then((reportingEvents) => {
    reportingEvents.forEach(function(reportingEvent) {
      addReportingEvent(reportingEvent);
    });
  });
  addWebUiListener('reporting-events-update', function(reportingEvent) {
    addReportingEvent(reportingEvent);
  });

  sendWithPromise('getDeepScans', []).then((requests) => {
    requests.forEach(function(request) {
      addDeepScan(request);
    });
  });
  addWebUiListener('deep-scan-request-update', function(result) {
    addDeepScan(result);
  });

  // <if expr="is_android">
  sendWithPromise('getReferringAppInfo', []).then((info) => {
    addReferringAppInfo(info);
  });
  // </if>

  $('get-referrer-chain-form').addEventListener('submit', addReferrerChain);

  sendWithPromise('getTailoredVerdictOverride', [])
      .then(displayTailoredVerdictOverride);
  addWebUiListener(
      'tailored-verdict-override-update', displayTailoredVerdictOverride);

  $('tailored-verdict-override-form')
      .addEventListener('submit', setTailoredVerdictOverride);
  $('tailored-verdict-override-clear')
      .addEventListener('click', clearTailoredVerdictOverride);

  // Allow tabs to be navigated to by fragment. The fragment with be of the
  // format "#tab-<tab id>"
  showTab(window.location.hash.substr(5));
  window.onhashchange = function() {
    showTab(window.location.hash.substr(5));
  };

  // When the tab updates, update the anchor
  $('tabbox').addEventListener('selected-index-change', e => {
    const tabs = document.querySelectorAll('div[slot=\'tab\']');
    const selectedTab = tabs[e.detail];
    window.location.hash = 'tab-' + selectedTab.id;
  }, true);
}

function addExperiments(result) {
  const resLength = result.length;

  for (let i = 0; i < resLength; i += 2) {
    const experimentsListFormatted =
        $('result-template').content.cloneNode(true);
    experimentsListFormatted.querySelectorAll('span')[0].textContent =
        result[i + 1] + ': ';
    experimentsListFormatted.querySelectorAll('span')[1].textContent =
        result[i];
    $('experiments-list').appendChild(experimentsListFormatted);
  }
}

function addPrefs(result) {
  const resLength = result.length;

  for (let i = 0; i < resLength; i += 2) {
    const preferencesListFormatted =
        $('result-template').content.cloneNode(true);
    preferencesListFormatted.querySelectorAll('span')[0].textContent =
        result[i + 1] + ': ';
    preferencesListFormatted.querySelectorAll('span')[1].textContent =
        result[i];
    $('preferences-list').appendChild(preferencesListFormatted);
  }
}

function addPolicies(result) {
  const resLength = result.length;

  for (let i = 0; i < resLength; i += 2) {
    const policiesListFormatted = $('result-template').content.cloneNode(true);
    policiesListFormatted.querySelectorAll('span')[0].textContent =
        result[i + 1] + ': ';
    policiesListFormatted.querySelectorAll('span')[1].textContent = result[i];
    $('policies-list').appendChild(policiesListFormatted);
  }
}

function addCookie(result) {
  const cookieFormatted = $('cookie-template').content.cloneNode(true);
  cookieFormatted.querySelectorAll('.result')[0].textContent = result[0];
  cookieFormatted.querySelectorAll('.result')[1].textContent =
      (new Date(result[1])).toLocaleString();
  $('cookie-panel').appendChild(cookieFormatted);
}

function addSavedPasswords(result) {
  const resLength = result.length;

  for (let i = 0; i < resLength; i += 2) {
    const savedPasswordFormatted = document.createElement('div');
    const suffix = result[i + 1] ? 'GAIA password' : 'Enterprise password';
    savedPasswordFormatted.textContent = `${result[i]} (${suffix})`;
    $('saved-passwords').appendChild(savedPasswordFormatted);
  }
}

function addDatabaseManagerInfo(result) {
  const resLength = result.length;

  for (let i = 0; i < resLength; i += 2) {
    const preferencesListFormatted =
        $('result-template').content.cloneNode(true);
    preferencesListFormatted.querySelectorAll('span')[0].textContent =
        result[i] + ': ';
    const value = result[i + 1];
    if (Array.isArray(value)) {
      const blockQuote = document.createElement('blockquote');
      value.forEach(item => {
        const div = document.createElement('div');
        div.textContent = item;
        blockQuote.appendChild(div);
      });
      preferencesListFormatted.querySelectorAll('span')[1].appendChild(
          blockQuote);
    } else {
      preferencesListFormatted.querySelectorAll('span')[1].textContent = value;
    }
    $('database-info-list').appendChild(preferencesListFormatted);
  }
}

function addFullHashCacheInfo(result) {
  $('full-hash-cache-info').textContent = result;
}

function addDownloadUrlChecked(url_and_result) {
  const logDiv = $('download-urls-checked-list');
  appendChildWithInnerText(logDiv, url_and_result);
}

function addSentClientDownloadRequestsInfo(result) {
  const logDiv = $('sent-client-download-requests-list');
  appendChildWithInnerText(logDiv, result);
}

function addReceivedClientDownloadResponseInfo(result) {
  const logDiv = $('received-client-download-response-list');
  appendChildWithInnerText(logDiv, result);
}

function addSentClientPhishingRequestsInfo(result) {
  const logDiv = $('sent-client-phishing-requests-list');
  appendChildWithInnerText(logDiv, result);
}

function addReceivedClientPhishingResponseInfo(result) {
  const logDiv = $('received-client-phishing-response-list');
  appendChildWithInnerText(logDiv, result);
}

function addSentCSBRRsInfo(result) {
  const logDiv = $('sent-csbrrs-list');
  appendChildWithInnerText(logDiv, result);
}

function addSentHitReportsInfo(result) {
  const logDiv = $('sent-hit-report-list');
  appendChildWithInnerText(logDiv, result);
}

function addPGEvent(result) {
  const logDiv = $('pg-event-log');
  const eventFormatted = '[' + (new Date(result['time'])).toLocaleString() +
      '] ' + result['message'];
  appendChildWithInnerText(logDiv, eventFormatted);
}

function addSecurityEvent(result) {
  const logDiv = $('security-event-log');
  const eventFormatted = '[' + (new Date(result['time'])).toLocaleString() +
      '] ' + result['message'];
  appendChildWithInnerText(logDiv, eventFormatted);
}

function insertTokenToTable(tableId, token) {
  const row = $(tableId).insertRow();
  row.className = 'content';
  row.id = tableId + '-' + token;
  row.insertCell().className = 'content';
  row.insertCell().className = 'content';
}

function addResultToTable(tableId, token, result, position) {
  if ($(tableId + '-' + token) === null) {
    insertTokenToTable(tableId, token);
  }

  const cell = $(tableId + '-' + token).cells[position];
  cell.innerText = result;
}

function addPGPing(result) {
  addResultToTable('pg-ping-list', result[0], result[1], 0);
}

function addPGResponse(result) {
  addResultToTable('pg-ping-list', result[0], result[1], 1);
}

function addURTLookupPing(result) {
  addResultToTable('urt-lookup-ping-list', result[0], result[1], 0);
}

function addURTLookupResponse(result) {
  addResultToTable('urt-lookup-ping-list', result[0], result[1], 1);
}

function addHPRTLookupPing(result) {
  addResultToTable('hprt-lookup-ping-list', result[0], result[1], 0);
}

function addHPRTLookupResponse(result) {
  addResultToTable('hprt-lookup-ping-list', result[0], result[1], 1);
}

function addDeepScan(result) {
  if (result['request_time'] != null) {
    const requestFormatted = '[' +
        (new Date(result['request_time'])).toLocaleString() + ']\n' +
        result['request'];
    addResultToTable('deep-scan-list', result['token'], requestFormatted, 0);
  }

  if (result['response_time'] != null) {
    if (result['response_status'] == 'SUCCESS') {
      // Display the response instead
      const resultFormatted = '[' +
          (new Date(result['response_time'])).toLocaleString() + ']\n' +
          result['response'];
      addResultToTable('deep-scan-list', result['token'], resultFormatted, 1);
    } else {
      // Display the error
      const resultFormatted = '[' +
          (new Date(result['response_time'])).toLocaleString() + ']\n' +
          result['response_status'];
      addResultToTable('deep-scan-list', result['token'], resultFormatted, 1);
    }
  }
}

function addLogMessage(result) {
  const logDiv = $('log-messages');
  const eventFormatted = '[' + (new Date(result['time'])).toLocaleString() +
      '] ' + result['message'];
  appendChildWithInnerText(logDiv, eventFormatted);
}

function addReportingEvent(result) {
  const logDiv = $('reporting-events');
  const eventFormatted = result['message'];
  appendChildWithInnerText(logDiv, eventFormatted);
}

function appendChildWithInnerText(logDiv, text) {
  if (!logDiv) {
    return;
  }
  const textDiv = document.createElement('div');
  textDiv.innerText = text;
  logDiv.appendChild(textDiv);
}

function addReferrerChain(ev) {
  // Don't navigate
  ev.preventDefault();

  sendWithPromise('getReferrerChain', $('referrer-chain-url').value)
      .then((response) => {
        $('referrer-chain-content').innerHTML = trustedTypes.emptyHTML;
        $('referrer-chain-content').textContent = response;
      });
}

// <if expr="is_android">
function addReferringAppInfo(info) {
  $('referring-app-info').innerHTML = trustedTypes.emptyHTML;
  $('referring-app-info').textContent = info;
}
// </if>

// Format the browser's response nicely.
function displayTailoredVerdictOverride(response) {
  let displayString = `Status: ${response.status}`;
  if (response.override_value) {
    displayString +=
        `\nOverride value: ${JSON.stringify(response.override_value)}`;
  }
  $('tailored-verdict-override-content').innerHTML = trustedTypes.emptyHTML;
  $('tailored-verdict-override-content').textContent = displayString;
}

function setTailoredVerdictOverride(e) {
  // Don't navigate
  e.preventDefault();

  const inputs = $('tailored-verdict-override-form').elements;

  // The structured data to send to the browser.
  const inputValue = {
    tailored_verdict_type: inputs['tailored_verdict_type'].value,
    adjustments: [],
  };
  inputs['adjustments'].forEach((checkbox) => {
    if (checkbox.checked) {
      inputValue.adjustments.push(checkbox.value);
    }
  });

  sendWithPromise('setTailoredVerdictOverride', inputValue)
      .then(displayTailoredVerdictOverride);
}

function clearTailoredVerdictOverride(e) {
  // Don't navigate
  e.preventDefault();

  $('tailored-verdict-override-form').reset();

  sendWithPromise('clearTailoredVerdictOverride')
      .then(displayTailoredVerdictOverride);
}

function showTab(tabId) {
  const tabs = document.querySelectorAll('div[slot=\'tab\']');
  const index = Array.from(tabs).findIndex(t => t.id === tabId);
  if (index !== -1) {
    document.querySelector('cr-tab-box')
        .setAttribute('selected-index', index.toString());
  }
}

document.addEventListener('DOMContentLoaded', initialize);
