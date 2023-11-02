// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This method initializes the two types of DOM storage.
function initDomStorage(value) {
  window.localStorage.setItem('foo', 'local-' + value);
  window.sessionStorage.setItem('bar', 'session-' + value);
}

// The code below is used for testing IndexedDB isolation.
// The test uses three basic operations -- open, read, write -- to verify proper
// isolation across webview tags with different storage partitions.
// Each of the basic functions below sends a postMessage to the embedder with
// the results.
var isolation = {};

isolation.db = null;
isolation.onerror = function(e) {
  sendResponse('error');
};

// Used to send postMessage to the embedder.
var channel = null;
// Id that the embedder uses to identify this guest.
var id = -1;

// This method opens the database and creates the objectStore if it doesn't
// exist. It sends a postMessage to the embedder with a string referring to
// which operation has been performed - open vs create.
function initIDB() {
  var v = 3;
  var ranVersionChangeTransaction = false;
  var request = indexedDB.open('isolation', v);
  request.onupgradeneeded = function(e) {
    isolation.db = e.target.result;
    var store = isolation.db.createObjectStore(
        'partitions', {keyPath: "id"});
    e.target.transaction.oncomplete = function() {
      ranVersionChangeTransaction = true;
    };
  }
  request.onsuccess = function(e) {
    isolation.db = e.target.result;
    if (ranVersionChangeTransaction) {
      sendResponse('idb created');
    } else {
      sendResponse('idb open');
    }
  };
  request.onerror = isolation.onerror;
  request.onblocked = isolation.onerror;
}

// This method adds a |value| to the database identified by |id|.
function addItemIDB(id, value) {
  var trans = isolation.db.transaction(['partitions'], 'readwrite');
  var store = trans.objectStore('partitions');
  var data = {'partition': value, 'id': id };

  var request = store.put(data);
  request.onsuccess = function(e) {
    sendResponse('addItemIDB complete');
  };
  request.onerror = isolation.onerror;
};

// This method reads an item from the database, and returns the result
// to the embedder using postMessage.
function readItemIDB(id) {
  var storedValue = null;
  var trans = isolation.db.transaction(['partitions'], 'readwrite');
  var store = trans.objectStore('partitions');

  var request = store.get(id);
  request.onsuccess = function(e) {
    if (!!e.target.result != false) {
      storedValue = request.result.partition;
    }
    sendResponse('readItemIDB: ' + storedValue);
  };
  request.onerror = isolation.onerror;
}

function openIDB(name) {
  var request = indexedDB.open('isolation');
  request.onsuccess = function(e) {
    e.target.result.version == 1 ? sendResponse('db not found')
                                 : sendResponse('db found');
  };
  request.onerror = isolation.onerror;
  request.onblocked = isolation.onerror;
}

function sendResponse(responseStr) {
  if (channel) {
    channel.postMessage(JSON.stringify([id, responseStr]), '*');
  }
}

window.onmessage = function(e) {
  if (!channel) {
    channel = e.source;
  }

  var data = JSON.parse(e.data);
  id = data.id;
  window.console.log('onmessage: ' + data.name);
  switch (data.name) {
    case 'init':
      initIDB();
      break;
    case 'add':
      addItemIDB(data.params[0], data.params[1]);
      break;
    case 'read':
      readItemIDB(data.params[0]);
      break;
    case 'open':
      openIDB(data.params[0]);
      break;
    default:
      sendResponse('bogus-command');  // Error.
      break;
  }
};
