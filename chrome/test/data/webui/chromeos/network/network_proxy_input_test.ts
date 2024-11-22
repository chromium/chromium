// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/network_proxy_input.js';

import type {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import type {NetworkProxyInputElement} from 'chrome://resources/ash/common/network/network_proxy_input.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('NetworkProxyInputTest', () => {
  let proxyInput: NetworkProxyInputElement;

  setup(async () => {
    proxyInput = document.createElement('network-proxy-input');
    document.body.appendChild(proxyInput);
    await flushTasks();
  });

  teardown(async () => {
    if (!proxyInput) {
      return;
    }
    proxyInput.remove();
    await flushTasks();
  });

  test('Focus input', async () => {
    assertTrue(!!proxyInput);
    proxyInput.focus();

    await flushTasks();
    const crInput = proxyInput.shadowRoot!.querySelector('cr-input');
    assertTrue(!!crInput);
    assertEquals(crInput, proxyInput.shadowRoot!.activeElement);
  });

  test('Dispatch proxy-input-change event', async () => {
    assertTrue(!!proxyInput);
    let proxyInputChangeEventCounter: number = 0;
    proxyInput.addEventListener('proxy-input-change', () => {
      proxyInputChangeEventCounter++;
    });
    const crInput = proxyInput.shadowRoot!.querySelector('cr-input');
    assertTrue(!!crInput);
    crInput.dispatchEvent(new Event('change'));

    await flushTasks();
    assertEquals(1, proxyInputChangeEventCounter);
  });

  test('Editable inputs', async () => {
    assertTrue(!!proxyInput);
    assertFalse(proxyInput.editable);
    const getHostCrInput = function() {
      const hostCrInput =
          proxyInput.shadowRoot!.querySelector<CrInputElement>('#host');
      assertTrue(!!hostCrInput);
      return hostCrInput;
    };
    const getPortCrInput = function() {
      const portCrInput =
          proxyInput.shadowRoot!.querySelector<CrInputElement>('#port');
      assertTrue(!!portCrInput);
      return portCrInput;
    };
    assertTrue(getHostCrInput().readonly);
    assertTrue(getPortCrInput().readonly);

    proxyInput.editable = true;
    await flushTasks();

    assertTrue(!!proxyInput);
    assertFalse(getHostCrInput().readonly);
    assertFalse(getPortCrInput().readonly);
  });
});
