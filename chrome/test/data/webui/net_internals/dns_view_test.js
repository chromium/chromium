// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DnsView} from 'chrome://net-internals/dns_view.js';
import {$} from 'chrome://resources/js/util_ts.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {Task, TaskQueue} from './task_queue.js';
import {switchToView} from './test_util.js';

window.dns_view_test = {};
const dns_view_test = window.dns_view_test;
dns_view_test.suiteName = 'NetInternalsDnsViewTests';
/** @enum {string} */
dns_view_test.TestNames = {
  ResolveHostWithoutAlternative: 'resolve host with no alternative endpoints',
  ResolveHostWithHTTP2Alternative:
      'resolve host with http2 alternative endpoint',
  ResolveHostWithHTTP3Alternative:
      'resolve host with http3 alternative endpoint',
  ResolveHostWithECHAlternative: 'resolve host with ECH alternative endpoint',
  ResolveHostWithMultipleAlternatives:
      'resolve host with multiple alternative endpoints',
  ErrorNameNotResolved: 'error name not resolved',
  ClearCache: 'clear cache',
};

suite(dns_view_test.suiteName, function() {
  /**
   * A Task that resolves the host by simulating a button click.
   * @extends {Task}
   */
  class ResolveHostTask extends Task {
    /**
     * @param {string} hostname The host address to attempt to look up.
     */
    constructor(hostname) {
      super();
      this.hostname_ = hostname;
    }

    start() {
      // enable mock network context for testing here
      chrome.send('setNetworkContextForTesting');
      const elementToObserve =
          document.getElementById('dns-view-dns-lookup-output');
      const options = {childList: true, subtree: true};
      const callback = () => {
        /* This condition is needed to avoid callbacking twice.*/
        if (elementToObserve.textContent !== '') {
          this.onTaskDone(elementToObserve.textContent);
          // disable mock network context for testing here
          chrome.send('resetNetworkContextForTesting');
        }
      };

      const observer = new MutationObserver(callback);
      observer.observe(elementToObserve, options);

      $(DnsView.DNS_LOOKUP_INPUT_ID).value = this.hostname_;
      $(DnsView.DNS_LOOKUP_SUBMIT_ID).click();
    }
  }

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
   * Checks a host resolve without alternative endpoints.
   */
  test(dns_view_test.TestNames.ResolveHostWithoutAlternative, function() {
    switchToView('dns');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(new ResolveHostTask('somewhere.com'));
    taskQueue.addFunctionTask(assertEquals.bind(
        null,
        'Resolved IP addresses of "somewhere.com": ["127.0.0.1"].' +
            'No alternative endpoints.'));
    return taskQueue.run();
  });

  /**
   * Checks a host resolve with an alternative endpoint that supports "http/1.1"
   * and "h2".
   */
  test(dns_view_test.TestNames.ResolveHostWithHTTP2Alternative, function() {
    switchToView('dns');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(new ResolveHostTask('http2.com'));
    taskQueue.addFunctionTask(assertEquals.bind(
        null,
        'Resolved IP addresses of "http2.com": ["127.0.0.1"].' +
            'Alternative endpoint: ' +
            '{"alpns":["http/1.1","h2"],"ip_endpoints":["127.0.0.1"]}.'));
    return taskQueue.run();
  });

  /**
   * Checks a host resolve with an alternative endpoint that supports
   * "http/1.1", "h2", and "h3".
   */
  test(dns_view_test.TestNames.ResolveHostWithHTTP3Alternative, function() {
    switchToView('dns');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(new ResolveHostTask('http3.com'));
    taskQueue.addFunctionTask(assertEquals.bind(
        null,
        'Resolved IP addresses of "http3.com": ["127.0.0.1"].' +
            'Alternative endpoint: ' +
            '{"alpns":["http/1.1","h2","h3"],"ip_endpoints":["127.0.0.1"]}.'));
    return taskQueue.run();
  });

  /**
   * Checks a host resolve with an alternative endpoint that supports ECH.
   */
  test(dns_view_test.TestNames.ResolveHostWithECHAlternative, function() {
    switchToView('dns');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(new ResolveHostTask('ech.com'));
    taskQueue.addFunctionTask(assertEquals.bind(
        null,
        'Resolved IP addresses of "ech.com": ["127.0.0.1"].' +
            'Alternative endpoint: ' +
            '{"alpns":["http/1.1","h2"],"ech_config_list":"AQIDBA==",' +
            '"ip_endpoints":["127.0.0.1"]}.'));
    return taskQueue.run();
  });

  /**
   * Checks a host resolve with multiple alternative endpoints.
   */
  test(dns_view_test.TestNames.ResolveHostWithMultipleAlternatives, function() {
    switchToView('dns');
    const taskQueue = new TaskQueue(true);
    taskQueue.addTask(new ResolveHostTask('multihost.com'));
    taskQueue.addFunctionTask(assertEquals.bind(
        null,
        'Resolved IP addresses of "multihost.com": ["127.0.0.1","127.0.0.2"].' +
            'Alternative endpoint: ' +
            '{"alpns":["http/1.1","h2"],"ip_endpoints":["127.0.0.1"]}.' +
            'Alternative endpoint: ' +
            '{"alpns":["http/1.1","h2","h3"],"ip_endpoints":["127.0.0.2"]}.'));
    return taskQueue.run();
  });

  /**
   * Checks an error when a host cannot be resolved.
   */
  test(dns_view_test.TestNames.ErrorNameNotResolved, function() {
    switchToView('dns');
    const taskQueue = new TaskQueue(true);

    // Make sure a lookup of unregistered hostname causes
    // net::ERR_NAME_NOT_RESOLVED.
    taskQueue.addTask(new ResolveHostTask('somewhere.org'));
    taskQueue.addFunctionTask(assertEquals.bind(
        null,
        'An error occurred while resolving "somewhere.org" (net::ERR_NAME_NOT_RESOLVED).'));
    return taskQueue.run();
  });

  /**
   * Adds a successful lookup to the DNS cache, then clears the cache.
   */
  test(dns_view_test.TestNames.ClearCache, function() {
    switchToView('dns');
    const taskQueue = new TaskQueue(true);

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
