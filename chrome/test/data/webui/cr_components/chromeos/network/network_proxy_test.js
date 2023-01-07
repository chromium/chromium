// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/network_proxy.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

suite('NetworkProxyTest', function() {
  /** @type {!NetworkProxy|undefined} */
  let netProxy;

  function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  setup(function() {
    netProxy = document.createElement('network-proxy');
    document.body.appendChild(netProxy);
    flush();
  });

  test('Proxy select option change fires proxy-change event', function(done) {
    const proxyType = netProxy.$.proxyType;

    // Verify that changing the proxy type select option fires the proxy-change
    // event with the new proxy type.
    netProxy.addEventListener('proxy-change', function(e) {
      assertEquals('WPAD', e.detail.type);
      done();
    });

    // Simulate changing the proxy select option.
    proxyType.value = 'WPAD';
    proxyType.dispatchEvent(new Event('change'));
  });

  test(
      'Add exception button only enabled when value in input',
      async function() {
        const button = netProxy.$.proxyExclusionButton;
        const input = netProxy.$.proxyExclusion;
        assertTrue(!!button);
        assertTrue(!!input);
        assertTrue(button.disabled);

        // Simulate typing a letter.
        input.value = 'A';

        await flushAsync();
        assertFalse(button.disabled);

        // Simulate deleting the letter.
        input.value = '';

        await flushAsync();
        assertTrue(button.disabled);
      });
});
