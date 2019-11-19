// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['net_internals_test.js']);

// Anonymous namespace
(function() {

/**
 * A Task that performs a DNS lookup.
 * @param {string} hostname The host address to attempt to look up.
 * @param {bool} local True if the lookup should be strictly local.
 * @extends {NetInternalsTest.Task}
 */
function DnsLookupTask(hostname, local) {
  this.hostname_ = hostname;
  this.local_ = local;
  NetInternalsTest.Task.call(this);
}

DnsLookupTask.prototype = {
  __proto__: NetInternalsTest.Task.prototype,

  start: function() {
    NetInternalsTest.setCallback(this.onTaskDone.bind(this));
    chrome.send('dnsLookup', [this.hostname_, this.local_]);
  },
};

/**
 * A Task that clears the cache by simulating a button click.
 * @extends {NetInternalsTest.Task}
 */
function ClearCacheTask() {
  NetInternalsTest.Task.call(this);
}

ClearCacheTask.prototype = {
  __proto__: NetInternalsTest.Task.prototype,

  start: function() {
    $(DnsView.CLEAR_CACHE_BUTTON_ID).onclick();
    this.onTaskDone();
  }
};

/**
 * Adds a successful lookup to the DNS cache, then clears the cache.
 */
TEST_F('NetInternalsTest', 'netInternalsDnsViewClearCache', function() {
  NetInternalsTest.switchToView('dns');
  var taskQueue = new NetInternalsTest.TaskQueue(true);

  // Perform an initial local lookup to make sure somewhere.com isn't cached.
  taskQueue.addTask(new DnsLookupTask('somewhere.com', true));
  taskQueue.addFunctionTask(expectEquals.bind(null, 'net::ERR_DNS_CACHE_MISS'));

  // Perform a non-local lookup to get somewhere.com added to the cache.
  taskQueue.addTask(new DnsLookupTask('somewhere.com', false));
  taskQueue.addFunctionTask(expectEquals.bind(null, '127.0.0.1'));

  // Perform another local lookup that should be cached this time.
  taskQueue.addTask(new DnsLookupTask('somewhere.com', true));
  taskQueue.addFunctionTask(expectEquals.bind(null, '127.0.0.1'));

  // Clear the cache
  taskQueue.addTask(new ClearCacheTask());

  // One more local lookup to make sure somewhere.com is no longer cached.
  taskQueue.addTask(new DnsLookupTask('somewhere.com', true));
  taskQueue.addFunctionTask(expectEquals.bind(null, 'net::ERR_DNS_CACHE_MISS'));

  taskQueue.run();
});
})();  // Anonymous namespace
