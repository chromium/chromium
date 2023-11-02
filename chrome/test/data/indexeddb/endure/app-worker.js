// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file simulates a typical background process of an offline-capable
// authoring application. When in an "online" state it receives chunks of
// data updates from a simulated server and stores them in a temporary IDB
// data store. On a different timer, the chunks are drained from the
// temporary store and combined into larger records in a permanent store.
// When in an "offline" state, nothing else happens.

function unexpectedErrorCallback(e) {
  self.postMessage({type: 'ERROR', error: {
    name: e.target.error.name,
    message: e.target.error.message
  }});
}

function unexpectedAbortCallback(e) {
  self.postMessage({type: 'ABORT', error: {
    name: e.target.error.name,
    message: e.target.error.message
  }});
}

function log(message) {
  self.postMessage({type: 'LOG', message: message});
}

function error(message) {
  self.postMessage({type: 'ERROR', message: message});
}

var DBNAME = 'endurance-db';
var DBVERSION = 1;

var MAX_DOC_ID = 25;
var MAX_CHUNK_ID = 10;
var MAX_CHUNK_SIZE = 5 * 1024;
var SYNC_TIMEOUT = 100;
var COMBINE_TIMEOUT = 234; // relatively prime with SYNC_TIMEOUT

function randomString(len)
{
  var s = '';
  while (len--)
    s += Math.floor((Math.random() * 36)).toString(36);
  return s;
}

var getNextChunk = (
  function () {
    var nextDocID = 0;
    var nextChunkID = 0;

    return function () {
      var doc_id = nextDocID;
      var chunk_id = nextChunkID;

      nextDocID += 1;
      if (nextDocID >= MAX_DOC_ID) {
        nextDocID = 0;
        nextChunkID += 1;
        if (nextChunkID >= MAX_CHUNK_ID)
          nextChunkID = 0;
      }

      return {
        docid: doc_id,
        chunkid: chunk_id,
        timestamp: new Date(),
        data: randomString(MAX_CHUNK_SIZE)
      };
    };
  }()
);


self.onmessage = function (event) {
  switch (event.data.type) {
    case 'offline':
      goOffline();
      break;
    case 'online':
      goOnline();
      break;
    default:
      throw new Error("Unexpected message: " + event.data.type);
  }
};


var offline = true;
var syncTimeoutId = 0;
var combineTimeoutId = 0;

function goOffline() {
  if (offline)
    return;
  log('offline');
  offline = true;
  clearTimeout(syncTimeoutId);
  syncTimeoutId = 0;
  clearTimeout(combineTimeoutId);
  combineTimeoutId = 0;
}

function goOnline() {
  if (!offline)
    return;
  offline = false;
  log('online');
  syncTimeoutId = setTimeout(sync, SYNC_TIMEOUT);
  combineTimeoutId = setTimeout(combine, COMBINE_TIMEOUT);
  // NOTE: Not using setInterval as we need to be sure they complete.
}

var sync_count = 0;
function sync() {
  if (offline)
    return;

  var sync_id = ++sync_count;
  log('sync ' + sync_id +  ' started');

  var chunk = getNextChunk();
  log('sync ' + sync_id +
      ' adding chunk: ' + chunk.chunkid +
      ' to doc: ' + chunk.docid);

  var request = indexedDB.open(DBNAME);
  request.onerror = unexpectedErrorCallback;
  request.onsuccess = function () {
    var db = request.result;
    if (db.version !== DBVERSION) {
      error('DB version incorrect');
      return;
    }

    var transaction = db.transaction('sync-chunks', 'readwrite');
    var store = transaction.objectStore('sync-chunks');
    request = store.put(chunk);
    transaction.onabort = unexpectedAbortCallback;
    transaction.oncomplete = function () {
      log('sync ' + sync_id +  ' finished');
      db.close();
      syncTimeoutId = setTimeout(sync, SYNC_TIMEOUT);
    };
  };
}

var combine_count = 0;
function combine() {
  if (offline)
    return;

  var combine_id = ++combine_count;
  log('combine ' + combine_id + ' started');

  var combine_chunk_count = 0;

  var request = indexedDB.open(DBNAME);
  request.onerror = unexpectedErrorCallback;
  request.onsuccess = function () {
    var db = request.result;
    if (db.version !== DBVERSION) {
      error('DB version incorrect');
      return;
    }

    var transaction = db.transaction(['sync-chunks', 'docs'], 'readwrite');
    var syncStore = transaction.objectStore('sync-chunks');
    var docStore = transaction.objectStore('docs');

    var cursorRequest = syncStore.openCursor();
    cursorRequest.onerror = unexpectedErrorCallback;
    cursorRequest.onsuccess = function () {
      var cursor = cursorRequest.result;
      if (cursor) {
        combine_chunk_count += 1;
        log('combine ' + combine_id +
            ' processing chunk # ' + combine_chunk_count);

        var key = cursor.key;
        var chunk = cursor.value;
        var docRequest = docStore.get(chunk.docid);
        docRequest.onerror = unexpectedErrorCallback;
        docRequest.onsuccess = function () {
          var doc = docRequest.result;
          if (!doc) {
            doc = {
              docid: chunk.docid,
              chunks: []
            };
            log('combine # ' + combine_id +
                ' created doc: ' + doc.docid);
          }

          log('combine # ' + combine_id +
              ' updating doc: ' + doc.docid +
              ' chunk: ' + chunk.chunkid);

          doc.chunks[chunk.chunkid] = chunk;
          doc.timestamp = new Date();
          request = docStore.put(doc);
          request.onerror = unexpectedErrorCallback;
          cursor.delete(key);
          cursor.continue();
        };
      } else {
        // let transaction complete
        log('combine ' + combine_id +
            ' done, processed ' + combine_chunk_count + ' chunks');
      }
    };
    transaction.onabort = unexpectedAbortCallback;
    transaction.oncomplete = function () {
      log('combine ' + combine_id +
          ' finished, processed ' + combine_chunk_count + ' chunks');
      db.close();
      combineTimeoutId = setTimeout(combine, COMBINE_TIMEOUT);
    };
  };
}
