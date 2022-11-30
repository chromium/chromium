// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file simulates a typical foreground process of an offline-capable
// authoring application. When in an "offline" state, simulated user actions
// are recorded for later playback in an IDB data store. When in an "online"
// state, the recorded actions are drained from the store (as if being sent
// to the server).

var $ = function(s) {
  return document.querySelector(s);
};

function status(message) {
  var elem = $('#status');
  while (elem.firstChild)
    elem.removeChild(elem.firstChild);
  elem.appendChild(document.createTextNode(message));
}

function log(message) {
  status(message);
}

function error(message) {
  status(message);
  console.error(message);
}

function unexpectedErrorCallback(e) {
  error("Unexpected error callback: (" + e.target.error.name + ") " +
        e.target.error.message);
}

function unexpectedAbortCallback(e) {
  error("Unexpected abort callback: (" + e.target.error.name + ") " +
        e.target.error.message);
}

function unexpectedBlockedCallback(e) {
  error("Unexpected blocked callback!");
}

var DBNAME = 'endurance-db';
var DBVERSION = 1;
var MAX_DOC_ID = 25;

var db;

function initdb() {
  var request = indexedDB.deleteDatabase(DBNAME);
  request.onerror = unexpectedErrorCallback;
  request.onblocked = unexpectedBlockedCallback;
  request.onsuccess = function () {
    request = indexedDB.open(DBNAME, DBVERSION);
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onupgradeneeded = function () {
      db = request.result;
      request.transaction.onabort = unexpectedAbortCallback;

      var syncStore = db.createObjectStore(
        'sync-chunks', {keyPath: 'sequence', autoIncrement: true});
      syncStore.createIndex('doc-index', 'docid');

      var docStore = db.createObjectStore(
        'docs', {keyPath: 'docid'});
      docStore.createIndex(
        'owner-index', 'owner', {multiEntry: true});

      var userEventStore = db.createObjectStore(
        'user-events', {keyPath: 'sequence', autoIncrement: true});
      userEventStore.createIndex('doc-index', 'docid');
    };
    request.onsuccess = function () {
      log('initialized');
      $('#offline').disabled = true;
      $('#online').disabled = false;
    };
  };
}

var offline = true;
var worker = new Worker('app-worker.js?cachebust');
worker.onmessage = function (event) {
  var data = event.data;
  switch (data.type) {
    case 'ABORT':
      unexpectedAbortCallback({target: {error: data.error}});
      break;
    case 'ERROR':
      unexpectedErrorCallback({target: {error: data.error}});
      break;
    case 'BLOCKED':
      unexpectedBlockedCallback({target: {error: data.error}});
      break;
    case 'LOG':
      log('WORKER: ' + data.message);
      break;
    case 'ERROR':
      error('WORKER: ' + data.message);
      break;
    }
};
worker.onerror = function (event) {
  error("Error in: " + event.filename + "(" + event.lineno + "): " +
        event.message);
};

$('#offline').addEventListener('click', goOffline);
$('#online').addEventListener('click', goOnline);

var EVENT_INTERVAL = 100;
var eventIntervalId = 0;

function goOffline() {
  if (offline)
    return;
  offline = true;
  $('#offline').disabled = offline;
  $('#online').disabled = !offline;
  $('#state').innerHTML = 'offline';
  log('offline');

  worker.postMessage({type: 'offline'});

  eventIntervalId = setInterval(recordEvent, EVENT_INTERVAL);
}

function goOnline() {
  if (!offline)
    return;
  offline = false;
  $('#offline').disabled = offline;
  $('#online').disabled = !offline;
  $('#state').innerHTML = 'online';
  log('online');

  worker.postMessage({type: 'online'});

  setTimeout(playbackEvents, 100);
  clearInterval(eventIntervalId);
  eventIntervalId = 0;
};

function recordEvent() {
  if (!db) {
    error("Database not initialized");
    return;
  }

  var transaction = db.transaction(['user-events'], 'readwrite');
  var store = transaction.objectStore('user-events');
  var record = {
    // 'sequence' key will be generated
    docid: Math.floor(Math.random() * MAX_DOC_ID),
    timestamp: new Date(),
    data: randomString(256)
  };

  log('putting user event');
  var request = store.put(record);
  request.onerror = unexpectedErrorCallback;
  transaction.onabort = unexpectedAbortCallback;
  transaction.oncomplete = function () {
    log('put user event');
  };
}

function sendEvent(record, callback) {
  setTimeout(
    function () {
      if (offline)
        callback(false);
      else {
        var serialization = JSON.stringify(record);
        callback(true);
      }
    },
    Math.random() * 200); // Simulate network jitter
}

var PLAYBACK_NONE = 0;
var PLAYBACK_SUCCESS = 1;
var PLAYBACK_FAILURE = 2;

function playbackEvent(callback) {
  log('playbackEvent');
  var result = false;
  var transaction = db.transaction(['user-events'], 'readonly');
  transaction.onabort = unexpectedAbortCallback;
  var store = transaction.objectStore('user-events');
  var cursorRequest = store.openCursor();
  cursorRequest.onerror = unexpectedErrorCallback;
  cursorRequest.onsuccess = function () {
    var cursor = cursorRequest.result;
    if (cursor) {
      var record = cursor.value;
      var key = cursor.key;
      // NOTE: sendEvent is asynchronous so transaction should finish
      sendEvent(
        record,
        function (success) {
          if (success) {
            // Use another transaction to delete event
            var transaction = db.transaction(['user-events'], 'readwrite');
            transaction.onabort = unexpectedAbortCallback;
            var store = transaction.objectStore('user-events');
            var deleteRequest = store.delete(key);
            deleteRequest.onerror = unexpectedErrorCallback;
            transaction.oncomplete = function () {
              // successfully sent and deleted event
              callback(PLAYBACK_SUCCESS);
            };
          } else {
            // No progress made
            callback(PLAYBACK_FAILURE);
          }
        });
    } else {
      callback(PLAYBACK_NONE);
    }
  };
}

var playback = false;

function playbackEvents() {
  log('playbackEvents');
  if (!db) {
    error("Database not initialized");
    return;
  }

  if (playback)
    return;

  playback = true;
  log("Playing back events");

  function nextEvent() {
    playbackEvent(
      function (result) {
        switch (result) {
          case PLAYBACK_NONE:
            playback = false;
            log("Done playing back events");
            return;
          case PLAYBACK_SUCCESS:
            setTimeout(nextEvent, 0);
            return;
          case PLAYBACK_FAILURE:
            playback = false;
            log("Failure during playback (dropped offline?)");
            return;
        }
      });
  }

  nextEvent();
}

function randomString(len) {
  var s = '';
  while (len--)
    s += Math.floor((Math.random() * 36)).toString(36);
  return s;
}

window.onload = function () {
  log("initializing...");
  initdb();
};
