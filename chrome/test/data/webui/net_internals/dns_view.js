// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['net_internals_test.js']);

// Anonymous namespace
(function() {

/**
 * A Task that adds a hostname to the cache and waits for it to appear in the
 * data we receive from the cache.
 * @param {string} hostname Name of host address we're waiting for.
 * @param {string} ipAddress IP address we expect it to have.  Null if we expect
 *     a net error other than OK.
 * @param {int} netError The expected network error code.
 * @param {bool} expired True if we expect the entry to be expired.  The added
 *     entry will have an expiration time far enough away from the current time
 *     that there will be no chance of any races.
 * @extends {NetInternalsTest.Task}
 */
function AddCacheEntryTask(hostname, ipAddress, netError, expired) {
  this.hostname_ = hostname;
  this.ipAddress_ = ipAddress;
  this.netError_ = netError;
  this.expired_ = expired;
  NetInternalsTest.Task.call(this);
}

AddCacheEntryTask.prototype = {
  __proto__: NetInternalsTest.Task.prototype,

  /**
   * Adds an entry to the cache and starts waiting to received the results from
   * the browser process.
   */
  start: function() {
    var addCacheEntryParams = [
      this.hostname_, this.ipAddress_, this.netError_, this.expired_ ? -2 : 2
    ];
    chrome.send('addCacheEntry', addCacheEntryParams);
    this.onTaskDone();
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
  taskQueue.addTask(
      new AddCacheEntryTask('somewhere.com', '1.2.3.4', 0, false));
  taskQueue.addTask(new ClearCacheTask());
  // TODO(mattm): verify that it was actually cleared.
  taskQueue.run(true);
});

})();  // Anonymous namespace
