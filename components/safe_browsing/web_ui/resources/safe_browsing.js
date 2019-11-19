/* Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

cr.ui.decorate('tabbox', cr.ui.TabBox);

cr.define('safe_browsing', function() {
  'use strict';
  /**
   * Asks the C++ SafeBrowsingUIHandler to get the lists of Safe Browsing
   * ongoing experiments and preferences.
   * The SafeBrowsingUIHandler should reply to addExperiment() and
   * addPreferences() (below).
   */
  function initialize() {
    cr.sendWithPromise('getExperiments', [])
        .then((experiments) => addExperiments(experiments));
    cr.sendWithPromise('getPrefs', []).then((prefs) => addPrefs(prefs));
    cr.sendWithPromise('getCookie', []).then((cookie) => addCookie(cookie));
    cr.sendWithPromise('getSavedPasswords', []).then((passwords) =>
        addSavedPasswords(passwords));
    cr.sendWithPromise('getDatabaseManagerInfo', []).then(
      function(databaseState) {
        var fullHashCacheState = databaseState.splice(-1,1);
        addDatabaseManagerInfo(databaseState);
        addFullHashCacheInfo(fullHashCacheState);
    });

    cr.sendWithPromise('getSentClientDownloadRequests', [])
        .then((sentClientDownloadRequests) => {
          sentClientDownloadRequests.forEach(function(cdr) {
            addSentClientDownloadRequestsInfo(cdr);
          });
        });
    cr.addWebUIListener(
        'sent-client-download-requests-update', function(result) {
          addSentClientDownloadRequestsInfo(result);
        });

    cr.sendWithPromise('getReceivedClientDownloadResponses', [])
        .then((receivedClientDownloadResponses) => {
          receivedClientDownloadResponses.forEach(function(cdr) {
            addReceivedClientDownloadResponseInfo(cdr);
          });
        });
    cr.addWebUIListener(
        'received-client-download-responses-update', function(result) {
          addReceivedClientDownloadResponseInfo(result);
        });

    cr.sendWithPromise('getSentCSBRRs', []).then((sentCSBRRs) => {
      sentCSBRRs.forEach(function(csbrr) {
        addSentCSBRRsInfo(csbrr);
      });
    });
    cr.addWebUIListener('sent-csbrr-update', function(result) {
      addSentCSBRRsInfo(result);
    });

    cr.sendWithPromise('getPGEvents', []).then((pgEvents) => {
      pgEvents.forEach(function(pgEvent) {
        addPGEvent(pgEvent);
      });
    });
    cr.addWebUIListener('sent-pg-event', function(result) {
      addPGEvent(result);
    });

    cr.sendWithPromise('getSecurityEvents', []).then((securityEvents) => {
      securityEvents.forEach(function(securityEvent) {
        addSecurityEvent(securityEvent);
      });
    });
    cr.addWebUIListener('sent-security-event', function(result) {
      addSecurityEvent(result);
    });

    cr.sendWithPromise('getPGPings', []).then((pgPings) => {
      pgPings.forEach(function(pgPing) {
        addPGPing(pgPing);
      });
    });
    cr.addWebUIListener('pg-pings-update', function(result) {
      addPGPing(result);
    });

    cr.sendWithPromise('getPGResponses', []).then((pgResponses) => {
      pgResponses.forEach(function(pgResponse) {
        addPGResponse(pgResponse);
      });
    });
    cr.addWebUIListener('pg-responses-update', function(result) {
      addPGResponse(result);
    });

    cr.sendWithPromise('getRTLookupPings', []).then((rtLookupPings) => {
      rtLookupPings.forEach(function(rtLookupPing) {
        addRTLookupPing(rtLookupPing);
      });
    });
    cr.addWebUIListener('rt-lookup-pings-update', function(result) {
      addRTLookupPing(result);
    });

    cr.sendWithPromise('getRTLookupResponses', []).then((rtLookupResponses) => {
      rtLookupResponses.forEach(function(rtLookupResponse) {
        addRTLookupResponse(rtLookupResponse);
      });
    });
    cr.addWebUIListener('rt-lookup-responses-update', function(result) {
      addRTLookupResponse(result);
    });

    cr.sendWithPromise('getLogMessages', []).then((logMessages) => {
      logMessages.forEach(function(message) {
        addLogMessage(message);
      });
    });
    cr.addWebUIListener('log-messages-update', function(message) {
      addLogMessage(message);
    });

    $('get-referrer-chain-form').addEventListener('submit', addReferrerChain);

    // Allow tabs to be navigated to by fragment. The fragment with be of the
    // format "#tab-<tab id>"
    showTab(window.location.hash.substr(5));
    window.onhashchange = function () {
      showTab(window.location.hash.substr(5));
    };

    // When the tab updates, update the anchor
    $('tabbox').addEventListener('selectedChange', function() {
      var tabbox = $('tabbox');
      var tabs = tabbox.querySelector('tabs').children;
      var selectedTab = tabs[tabbox.selectedIndex];
      window.location.hash = 'tab-' + selectedTab.id;
    }, true);
  }

  function addExperiments(result) {
    var resLength = result.length;
    var experimentsListFormatted = '';

    for (var i = 0; i < resLength; i += 2) {
      experimentsListFormatted += "<div><b>" + result[i + 1] +
          "</b>: " + result[i] + "</div>";
    }
    $('experiments-list').innerHTML = experimentsListFormatted;
  }

  function addPrefs(result) {
    var resLength = result.length;
    var preferencesListFormatted = "";

    for (var i = 0; i < resLength; i += 2) {
      preferencesListFormatted += "<div><b>" + result[i + 1] + "</b>: " +
          result[i] + "</div>";
    }
    $('preferences-list').innerHTML = preferencesListFormatted;
  }

  function addCookie(result) {
    var cookieFormatted = '<b>Value:</b> ' + result[0] + '\n' +
        '<b>Created:</b> ' + (new Date(result[1])).toLocaleString();
    $('cookie-panel').innerHTML = cookieFormatted;
  }

  function addSavedPasswords(result) {
    var resLength = result.length;
    var savedPasswordFormatted = "";

    for (var i = 0; i < resLength; i += 2) {
      savedPasswordFormatted += "<div>" + result[i];
      if (result[i+1]) {
        savedPasswordFormatted += " (GAIA password)";
      } else {
        savedPasswordFormatted += " (Enterprise password)";
      }
      savedPasswordFormatted += "</div>";
    }

    $('saved-passwords').innerHTML = savedPasswordFormatted;
  }

  function addDatabaseManagerInfo(result) {
    var resLength = result.length;
    var preferencesListFormatted = "";

    for (var i = 0; i < resLength; i += 2) {
      preferencesListFormatted += "<div><b>" + result[i] + "</b>: " +
          result[i + 1] + "</div>";
    }
    $('database-info-list').innerHTML = preferencesListFormatted;
  }

  function addFullHashCacheInfo(result) {
    $('full-hash-cache-info').innerHTML = result;
  }

  function addSentClientDownloadRequestsInfo(result) {
    var logDiv = $('sent-client-download-requests-list');
    appendChildWithInnerText(logDiv, result);
  }

  function addReceivedClientDownloadResponseInfo(result) {
    var logDiv = $('received-client-download-response-list');
    appendChildWithInnerText(logDiv, result);
  }

  function addSentCSBRRsInfo(result) {
    var logDiv = $('sent-csbrrs-list');
    appendChildWithInnerText(logDiv, result);
  }

  function addPGEvent(result) {
    var logDiv = $('pg-event-log');
    var eventFormatted = "[" + (new Date(result["time"])).toLocaleString() +
        "] " + result['message'];
    appendChildWithInnerText(logDiv, eventFormatted);
  }

  function addSecurityEvent(result) {
    var logDiv = $('security-event-log');
    var eventFormatted = "[" + (new Date(result["time"])).toLocaleString() +
        "] " + result['message'];
    appendChildWithInnerText(logDiv, eventFormatted);
  }

  function insertTokenToTable(tableId, token) {
    var row = $(tableId).insertRow();
    row.className = 'content';
    row.id = tableId + '-' + token;
    row.insertCell().className = 'content';
    row.insertCell().className = 'content';
  }

  function addResultToTable(tableId, result, position) {
    var token = result[0];
    var request = result[1];

    if ($(tableId + '-' + token) == undefined) {
      insertTokenToTable(tableId, token);
    }

    var cell = $(tableId + '-' + token).cells[position];
    appendChildWithInnerText(cell, request);
  }

  function addPGPing(result) {
    addResultToTable('pg-ping-list', result, 0);
  }

  function addPGResponse(result) {
    addResultToTable('pg-ping-list', result, 1);
  }

  function addRTLookupPing(result) {
    addResultToTable('rt-lookup-ping-list', result, 0);
  }

  function addRTLookupResponse(result) {
    addResultToTable('rt-lookup-ping-list', result, 1);
  }

  function addLogMessage(result) {
    var logDiv = $('log-messages');
    var eventFormatted = "[" + (new Date(result["time"])).toLocaleString() +
        "] " + result['message'];
    appendChildWithInnerText(logDiv, eventFormatted);
  }

  function appendChildWithInnerText(logDiv, text) {
    if (!logDiv) {
      return;
    }
    var textDiv = document.createElement('div');
    textDiv.innerText = text;
    logDiv.appendChild(textDiv);
  }

  function addReferrerChain(ev) {
    // Don't navigate
    ev.preventDefault();

    cr.sendWithPromise('getReferrerChain', $('referrer-chain-url').value)
        .then((response) => {
          $('referrer-chain-content').innerHTML = response;
        });
  }

  function showTab(tabId) {
    if ($(tabId)) {
      $(tabId).selected = "selected";
    }
  }

  return {
    addSentCSBRRsInfo: addSentCSBRRsInfo,
    addSentClientDownloadRequestsInfo: addSentClientDownloadRequestsInfo,
    addReceivedClientDownloadResponseInfo:
        addReceivedClientDownloadResponseInfo,
    addPGEvent: addPGEvent,
    addSecurityEvent: addSecurityEvent,
    addPGPing: addPGPing,
    addPGResponse: addPGResponse,
    initialize: initialize,
  };
});

document.addEventListener('DOMContentLoaded', safe_browsing.initialize);
