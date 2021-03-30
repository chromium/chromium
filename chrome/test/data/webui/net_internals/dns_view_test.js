// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DnsView} from 'chrome://net-internals/dns_view.js';
import {$} from 'chrome://resources/js/util.m.js';

import {assertEquals} from '../chai_assert.js';

import {Task, TaskQueue} from './task_queue.js';
import {switchToView} from './test_util.js';

suite('NetInternalsDnsViewTests', function() {
  /**
   * A Task that performs a DNS lookup.
   * @extends {Task}
   */
  class DnsLookupTask extends Task {
    /**
     * @param {string} hostname The host address to attempt to look up.
     * @param {bool} local True if the lookup should be strictly local.
     */
    constructor(hostname, local) {
      super();
      this.hostname_ = hostname;
      this.local_ = local;
    }

    start() {
      NetInternalsTest.setCallback(this.onTaskDone.bind(this));
      chrome.send('dnsLookup', [this.hostname_, this.local_]);
    }
  }

  /**
   * A Task that clears the cache by simulating a button click.
   * @extends {Task}
   */
  class ClearCacheTask extends Task {
    start() {
      $(DnsView.CLEAR_CACHE_BUTTON_ID).onclick();
      this.onTaskDone();
    }
  }

  /**
   * Adds a successful lookup to the DNS cache, then clears the cache.
   */
  test('clear cache', function() {
    switchToView('dns');
    var taskQueue = new TaskQueue(true);

    // Perform an initial local lookup to make sure somewhere.com isn't cached.
    taskQueue.addTask(new DnsLookupTask('somewhere.com', true));
    taskQueue.addFunctionTask(
        assertEquals.bind(null, 'net::ERR_DNS_CACHE_MISS'));

    // Perform a non-local lookup to get somewhere.com added to the cache.
    taskQueue.addTask(new DnsLookupTask('somewhere.com', false));
    taskQueue.addFunctionTask(assertEquals.bind(null, '127.0.0.1'));

    // Perform another local lookup that should be cached this time.
    taskQueue.addTask(new DnsLookupTask('somewhere.com', true));
    taskQueue.addFunctionTask(assertEquals.bind(null, '127.0.0.1'));

    // Clear the cache
    taskQueue.addTask(new ClearCacheTask());

    // One more local lookup to make sure somewhere.com is no longer cached.
    taskQueue.addTask(new DnsLookupTask('somewhere.com', true));
    taskQueue.addFunctionTask(
        assertEquals.bind(null, 'net::ERR_DNS_CACHE_MISS'));

    return taskQueue.run();
  });
});
