// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DnsView} from 'chrome://net-internals/dns_view.js';
import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {$} from 'chrome://resources/js/util.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {switchToView} from './test_util.js';

suite('NetInternalsDnsViewTest', function() {
  // Resolves the host by simulating a button click.
  function resolveHost(hostname) {
    return new Promise(resolve => {
      // enable mock network context for testing here
      chrome.send('setNetworkContextForTesting');
      const elementToObserve =
          document.getElementById('dns-view-dns-lookup-output');
      const options = {childList: true, subtree: true};
      const callback = () => {
        /* This condition is needed to avoid callbacking twice.*/
        if (elementToObserve.textContent !== '') {
          // disable mock network context for testing here
          chrome.send('resetNetworkContextForTesting');
          resolve(elementToObserve.textContent);
        }
      };

      const observer = new MutationObserver(callback);
      observer.observe(elementToObserve, options);

      $(DnsView.DNS_LOOKUP_INPUT_ID).value = hostname;
      $(DnsView.DNS_LOOKUP_SUBMIT_ID).click();
    });
  }

  /**
   * Performs a DNS lookup.
   * @param {string} hostname The host address to attempt to look up.
   * @param {bool} local True if the lookup should be strictly local.
   * @return {!Promise<string>}
   */
  function dnsLookup(hostname, local) {
    return sendWithPromise('dnsLookup', hostname, local);
  }

  // Clears the cache by simulating a button click.
  function clearCache() {
    $(DnsView.CLEAR_CACHE_BUTTON_ID).onclick();
  }

  /**
   * Checks a host resolve without alternative endpoints.
   */
  test('ResolveHostWithoutAlternative', async function() {
    switchToView('dns');
    const result = await resolveHost('somewhere.com');
    assertEquals(
        'Resolved IP addresses of "somewhere.com": ["127.0.0.1"].' +
            'No alternative endpoints.',
        result);
  });

  /**
   * Checks a host resolve with an alternative endpoint that supports "http/1.1"
   * and "h2".
   */
  test(
      'ResolveHostWithHTTP2Alternative', async function() {
        switchToView('dns');
        const result = await resolveHost('http2.com');
        assertEquals(
            'Resolved IP addresses of "http2.com": ["127.0.0.1"].' +
                'Alternative endpoint: ' +
                '{"alpns":["http/1.1","h2"],"ip_endpoints":["127.0.0.1"]}.',
            result);
      });

  /**
   * Checks a host resolve with an alternative endpoint that supports
   * "http/1.1", "h2", and "h3".
   */
  test(
      'ResolveHostWithHTTP3Alternative', async function() {
        switchToView('dns');
        const result = await resolveHost('http3.com');
        assertEquals(
            'Resolved IP addresses of "http3.com": ["127.0.0.1"].' +
                'Alternative endpoint: ' +
                '{"alpns":["http/1.1","h2","h3"],"ip_endpoints":["127.0.0.1"]}.',
            result);
      });

  /**
   * Checks a host resolve with an alternative endpoint that supports ECH.
   */
  test('ResolveHostWithECHAlternative', async function() {
    switchToView('dns');
    const result = await resolveHost('ech.com');
    assertEquals(
        'Resolved IP addresses of "ech.com": ["127.0.0.1"].' +
            'Alternative endpoint: ' +
            '{"alpns":["http/1.1","h2"],"ech_config_list":"AQIDBA==",' +
            '"ip_endpoints":["127.0.0.1"]}.',
        result);
  });

  /**
   * Checks a host resolve with multiple alternative endpoints.
   */
  test(
      'ResolveHostWithMultipleAlternatives', async function() {
        switchToView('dns');
        const result = await resolveHost('multihost.com');
        assertEquals(
            'Resolved IP addresses of "multihost.com": ["127.0.0.1","127.0.0.2"].' +
                'Alternative endpoint: ' +
                '{"alpns":["http/1.1","h2"],"ip_endpoints":["127.0.0.1"]}.' +
                'Alternative endpoint: ' +
                '{"alpns":["http/1.1","h2","h3"],"ip_endpoints":["127.0.0.2"]}.',
            result);
      });

  /**
   * Checks an error when a host cannot be resolved.
   */
  test('ErrorNameNotResolved', async function() {
    switchToView('dns');
    // Make sure a lookup of unregistered hostname causes
    // net::ERR_NAME_NOT_RESOLVED.
    const result = await resolveHost('somewhere.org');
    assertEquals(
        'An error occurred while resolving "somewhere.org" (net::ERR_NAME_NOT_RESOLVED).',
        result);
  });

  /**
   * Adds a successful lookup to the DNS cache, then clears the cache.
   */
  test('ClearCache', async function() {
    switchToView('dns');

    // Perform an initial local lookup to make sure somewhere.com isn't cached.
    let result = await dnsLookup('somewhere.com', true);
    assertEquals('net::ERR_DNS_CACHE_MISS', result);

    // Perform a non-local lookup to get somewhere.com added to the cache.
    result = await dnsLookup('somewhere.com', false);
    assertEquals('127.0.0.1', result);

    // Perform another local lookup that should be cached this time.
    result = await dnsLookup('somewhere.com', true);
    assertEquals('127.0.0.1', result);

    // Clear the cache
    clearCache();

    // One more local lookup to make sure somewhere.com is no longer cached.
    result = await dnsLookup('somewhere.com', true);
    assertEquals('net::ERR_DNS_CACHE_MISS', result);
  });
});
