// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/network_proxy.js';

import type {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import type {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import type {NetworkProxyElement} from 'chrome://resources/ash/common/network/network_proxy.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('NetworkProxyTest', () => {
  let netProxy: NetworkProxyElement|undefined;

  setup(() => {
    netProxy = document.createElement('network-proxy');
    document.body.appendChild(netProxy);
    flush();
  });

  test('Proxy select option change fires proxy-change event', (done) => {
    assertTrue(!!netProxy);
    const proxyType =
        netProxy.shadowRoot!.querySelector<HTMLSelectElement>('#proxyType');

    // Verify that changing the proxy type select option fires the proxy-change
    // event with the new proxy type.
    netProxy.addEventListener('proxy-change', function(e: Event) {
      const customEvent = e as CustomEvent;
      assertEquals('WPAD', customEvent.detail.type);
      done();
    });

    // Simulate changing the proxy select option.
    assertTrue(!!proxyType);
    proxyType.value = 'WPAD';
    proxyType.dispatchEvent(new Event('change'));
  });

  test('Add exception button only enabled when value in input', async () => {
    assertTrue(!!netProxy);
    const button = netProxy.shadowRoot!.querySelector<CrButtonElement>(
        '#proxyExclusionButton');
    const input =
        netProxy.shadowRoot!.querySelector<CrInputElement>('#proxyExclusion');
    assertTrue(!!button);
    assertTrue(!!input);
    assertTrue(button.disabled);

    // Simulate typing a letter.
    input.value = 'A';

    await flushTasks();
    assertFalse(button.disabled);

    // Simulate deleting the letter.
    input.value = '';

    await flushTasks();
    assertTrue(button.disabled);
  });
});
