// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var automation = {
  results: {}
};

automation.setDone = function() {
  this.setStatus("Test complete.");
  document.cookie = '__done=1; path=/';
};

automation.addResult = function(name, result) {
  result = "" + result;
  this.results[name] = result;
  var elt = document.getElementById('results');
  var div = document.createElement('div');
  div.textContent = name + ": " + result;
  elt.appendChild(div);
};

automation.getResults = function() {
  return this.results;
};

automation.setStatus = function(s) {
  document.getElementById('status').textContent = s;
};

function assert(t) {
  if (!t) {
    var e = new Error("Assertion failed!");
    console.log(e.stack);
    throw e;
  }
}

function onError(e) {
  var s = "Caught error.";
  if (e.target && e.target.error)
    s += "\n" + e.target.error.name + "\n" + e.target.error.message;
  console.log(s);
  automation.setStatus(s);
  e.stopPropagation();
  throw new Error(e);
}

var baseVersion = 2;  // The version with our object stores.
var curVersion;

// Valid options fields:
//  indexName: the name of an index to create on each object store
//  indexKeyPath: the key path for that index
//  indexIsUnique: the "unique" option for IDBIndexParameters
//  indexIsMultiEntry: the "multiEntry" option for IDBIndexParameters
//
function createDatabase(
    name, objectStoreNames, handler, errorHandler, optionSets) {
  var openRequest = indexedDB.open(name, baseVersion);
  openRequest.onblocked = errorHandler;
  openRequest.onerror = errorHandler;
  function createObjectStores(db) {
    for (var store in objectStoreNames) {
      var name = objectStoreNames[store];
      assert(!db.objectStoreNames.contains(name));
      var os = db.createObjectStore(name);
      if (optionSets) {
        for (o in optionSets) {
          var options = optionSets[o];
          assert(options.indexName);
          assert('indexKeyPath' in options);
          os.createIndex(options.indexName, options.indexKeyPath,
              { unique: options.indexIsUnique,
                multiEntry: options.indexIsMultiEntry });
        }
      }
    }
  }
  openRequest.onupgradeneeded = function(ev) {
    // This is the spec-compliant path, which doesn't yet run in Chrome, but
    // works in Firefox.
    assert(openRequest == ev.target);
    var db = openRequest.result;
    db.onerror = errorHandler;
    createObjectStores(db);
    // onsuccess will get called after this exits.
  };
  openRequest.onsuccess = function(ev) {
    assert(openRequest == ev.target);
    var db = openRequest.result;
    curVersion = db.version;
    db.onerror = function(ev) {
      console.log("db error", arguments, openRequest.error.message);
      errorHandler(ev);
    };
    if (curVersion != baseVersion) {
      // This is the legacy path, which runs only in Chrome.
      var setVersionRequest = db.setVersion(baseVersion);
      setVersionRequest.onerror = errorHandler;
      setVersionRequest.onsuccess = function(e) {
        assert(setVersionRequest == e.target);
        createObjectStores(db);
        var versionTransaction = setVersionRequest.result;
        versionTransaction.oncomplete = function() { handler(db); };
        versionTransaction.onerror = onError;
      };
    } else {
      handler(db);
    }
  };
}

// You must close all database connections before calling this.
function alterObjectStores(
    name, objectStoreNames, func, handler, errorHandler) {
  var version = curVersion + 1;
  var openRequest = indexedDB.open(name, version);
  openRequest.onblocked = errorHandler;
  openRequest.onupgradeneeded = function(ev) {
    doAlteration(ev.target.transaction);
    // onsuccess will get called after this exits.
  };
  openRequest.onsuccess = function(ev) {
    assert(openRequest == ev.target);
    var db = openRequest.result;
    db.onerror = function(ev) {
      console.log("error altering db", arguments,
          openRequest.error.message);
      errorHandler();
    };
    if (db.version != version) {
      // This is the legacy path, which runs only in Chrome before M23.
      var setVersionRequest = db.setVersion(version);
      setVersionRequest.onerror = errorHandler;
      setVersionRequest.onsuccess =
          function(e) {
            curVersion = db.version;
            assert(setVersionRequest == e.target);
            var versionTransaction = setVersionRequest.result;
            versionTransaction.oncomplete = function() { handler(db); };
            versionTransaction.onerror = onError;
            doAlteration(versionTransaction);
          };
    } else {
      handler(db);
    }
  };
  function doAlteration(target) {
    for (var store in objectStoreNames) {
      func(target.objectStore(objectStoreNames[store]));
    }
  }
}

function getTransaction(db, objectStoreNames, mode, opt_handler) {
  var transaction = db.transaction(objectStoreNames, mode);
  transaction.onerror = onError;
  transaction.onabort = onError;
  if (opt_handler) {
    transaction.oncomplete = opt_handler;
  }
  return transaction;
}

function deleteDatabase(name, opt_handler) {
  var deleteRequest = indexedDB.deleteDatabase(name);
  deleteRequest.onerror = onError;
  deleteRequest.onblocked = onError;
  if (opt_handler) {
    deleteRequest.onsuccess = opt_handler;
  }
}

function getCompletionFunc(db, testName, startTime, onTestComplete) {
  function onDeleted() {
    automation.setStatus("Deleted database.");
    onTestComplete();
  }
  return function() {
    var duration = window.performance.now() - startTime;
    // Ignore the cleanup time for this test.
    automation.addResult(testName, duration);
    automation.setStatus("Deleting database.");
    db.close();
    deleteDatabase(testName, onDeleted);
  };
}

function getDisplayName(args) {
  function functionName(f) {
    // Function.prototype.name is nonstandard, and not implemented in IE10-
    return f.name || f.toString().match(/^function\s*([^(\s]*)/)[1];
  }
  // The last arg is the completion callback the test runner tacks on.
  // TODO(ericu): Make test errors delete the database automatically.
  return functionName(getDisplayName.caller) + (args.length > 1 ? "_" : "") +
      Array.prototype.slice.call(args, 0, args.length - 1).join("_");
}

// Pad a string [or object convertible to a string] to a fixed width; use this
// to have numeric strings sort properly.
function padToWidth(s, width) {
  s = String(s);
  assert(s.length <= width);
  if (s.length < width) {
    s = stringOfLength(width - s.length, '0') + s;
  }
  return s;
}

function stringOfLength(n, c) {
  if (c == null)
    c = 'X';
  assert(n > 0);
  assert(n == Math.floor(n));
  return new Array(n + 1).join(c);
}

function getSimpleKey(i) {
  return "key " + padToWidth(i, 10);
}

function getSimpleValue(i) {
  return "value " + padToWidth(i, 10);
}

function getIndexableValue(i) {
  return { id: getSimpleValue(i) };
}

function getForwardIndexKey(i) {
  return i;
}

function getBackwardIndexKey(i) {
  return -i;
}

// This is useful for indexing by keypath; the two names should be ordered in
// opposite directions for all i in uint32 range.
function getObjectValue(i) {
  return {
    firstName: getForwardIndexKey(i),
    lastName: getBackwardIndexKey(i)
  };
}

function getNFieldName(k) {
  return "field" + k;
}

function getNFieldObjectValue(i, n) {
  assert(Math.floor(n) == n);
  assert(n > 0);
  var o = {};
  for (; n > 0; --n) {
    // The value varies per field, each object will tend to be unique,
    // and thanks to the modulus, indexing on different fields will give you
    // different ordering for large-enough data sets.
    o[getNFieldName(n - 1)] = Math.pow(i + 0.5, n + 0.5) % 65536;
  }
  return o;
}

function putLinearValues(
    transaction, objectStoreNames, numKeys, getKey, getValue) {
  if (!getKey)
    getKey = getSimpleKey;
  if (!getValue)
    getValue = getSimpleValue;
  for (var i in objectStoreNames) {
    var os = transaction.objectStore(objectStoreNames[i]);
    for (var j = 0; j < numKeys; ++j) {
      var request = os.put(getValue(j), getKey(j));
      request.onerror = onError;
    }
  }
}

function verifyResultNonNull(result) {
  assert(result != null);
}

function getRandomValues(
    transaction, objectStoreNames, numReads, numKeys, indexName, getKey) {
  if (!getKey)
    getKey = getSimpleKey;
  for (var i in objectStoreNames) {
    var os = transaction.objectStore(objectStoreNames[i]);
    var source = os;
    if (indexName)
      source = source.index(indexName);
    for (var j = 0; j < numReads; ++j) {
      var rand = Math.floor(random() * numKeys);
      var request = source.get(getKey(rand));
      request.onerror = onError;
      request.onsuccess = verifyResultNonNull;
    }
  }
}

function putRandomValues(
    transaction, objectStoreNames, numPuts, numKeys, getKey, getValue) {
  if (!getKey)
    getKey = getSimpleKey;
  if (!getValue)
    getValue = getSimpleValue;
  for (var i in objectStoreNames) {
    var os = transaction.objectStore(objectStoreNames[i]);
    for (var j = 0; j < numPuts; ++j) {
      var rand = Math.floor(random() * numKeys);
      var request = os.put(getValue(rand), getKey(rand));
      request.onerror = onError;
    }
  }
}

function getSpecificValues(transaction, objectStoreNames, indexName, keys) {
  for (var i in objectStoreNames) {
    var os = transaction.objectStore(objectStoreNames[i]);
    var source = os;
    if (indexName)
      source = source.index(indexName);
    for (var j = 0; j < keys.length; ++j) {
      var request = source.get(keys[j]);
      request.onerror = onError;
      request.onsuccess = verifyResultNonNull;
    }
  }
}

// getKey should be deterministic, as we assume that a cursor that starts at
// getKey(X) and runs through getKey(X + K) has exactly K values available.
// This is annoying to guarantee generally when using an index, so we avoid both
// ends of the key space just in case and use simple indices.
// TODO(ericu): Figure out if this can be simplified and we can remove uses of
// getObjectValue in favor of getNFieldObjectValue.
function getValuesFromCursor(
    transaction, inputObjectStoreName, numReads, numKeys, indexName, getKey,
    readKeysOnly, outputObjectStoreName) {
  assert(2 * numReads < numKeys);
  if (!getKey)
    getKey = getSimpleKey;
  var rand = Math.floor(random() * (numKeys - 2 * numReads)) + numReads;
  var values = [];
  var queryObject = transaction.objectStore(inputObjectStoreName);
  assert(queryObject);
  if (indexName)
    queryObject = queryObject.index(indexName);
  var keyRange = IDBKeyRange.bound(
      getKey(rand), getKey(rand + numReads), false, true);
  var request;
  if (readKeysOnly) {
    request = queryObject.openKeyCursor(keyRange);
  } else {
    request = queryObject.openCursor(keyRange);
  }
  var oos;
  if (outputObjectStoreName)
    oos = transaction.objectStore(outputObjectStoreName);
  var numReadsLeft = numReads;
  request.onsuccess = function(event) {
    var cursor = event.target.result;
    if (cursor) {
      assert(numReadsLeft);
      --numReadsLeft;
      if (oos)
        // Put in random order for maximum difficulty.  We add in numKeys just
        // in case we're writing back to the same store; this way we won't
        // affect the number of keys available to the cursor, since we're always
        // outside its range.
        oos.put(cursor.value, numKeys + random());
      values.push({key: cursor.key, value: cursor.value});
      cursor.continue();
    } else {
      assert(!numReadsLeft);
    }
  };
  request.onerror = onError;
}

function runTransactionBatch(db, count, batchFunc, objectStoreNames, mode,
    onComplete) {
  var numTransactionsRunning = 0;

  runOneBatch(db);

  function runOneBatch(db) {
    if (count <= 0) {
      return;
    }
    --count;
    ++numTransactionsRunning;
    var transaction = getTransaction(db, objectStoreNames, mode,
        function() {
          assert(!--numTransactionsRunning);
          if (count <= 0) {
            onComplete();
          } else {
            runOneBatch(db);
          }
        });

    batchFunc(transaction);
  }
}

// Use random() instead of Math.random() so runs are repeatable.
var random = (function(seed) {

  // Implementation of: http://www.burtleburtle.net/bob/rand/smallprng.html
  function uint32(x) { return x >>> 0; }
  function rot(x, k) { return (x << k) | (x >> (32 - k)); }

  function SmallPRNG(seed) {
    seed = uint32(seed);
    this.a = 0xf1ea5eed;
    this.b = this.c = this.d = seed;
    for (var i = 0; i < 20; ++i)
      this.ranval();
  }

  SmallPRNG.prototype.ranval = function() {
    var e = uint32(this.a - rot(this.b, 27));
    this.a = this.b ^ rot(this.c, 17);
    this.b = uint32(this.c + this.d);
    this.c = uint32(this.d + e);
    this.d = uint32(e + this.a);
    return this.d;
  };

  var prng = new SmallPRNG(seed);
  return function() { return prng.ranval() / 0x100000000; };
}(0));
