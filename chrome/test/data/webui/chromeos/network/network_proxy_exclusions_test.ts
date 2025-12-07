// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/network_proxy_exclusions.js';

import type {NetworkProxyExclusionsElement} from 'chrome://resources/ash/common/network/network_proxy_exclusions.js';
import {assertDeepEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('NetworkProxyExclusionsTest', () => {
  let proxyExclusions: NetworkProxyExclusionsElement;

  setup(async () => {
    proxyExclusions = document.createElement('network-proxy-exclusions');
    document.body.appendChild(proxyExclusions);
    await flushTasks();
  });

  teardown(async () => {
    if (!proxyExclusions) {
      return;
    }
    proxyExclusions.remove();
    await flushTasks();
  });

  test('Clear fires proxy-exclusions-change event', async () => {
    proxyExclusions.exclusions = [
      'one',
      'two',
      'three',
    ];
    await flushTasks();

    const clearBtn =
        proxyExclusions.shadowRoot!.querySelector('cr-icon-button');
    assertTrue(!!clearBtn);

    // Verify that clicking the clear button fires the proxy-exclusions-change
    // event and that the item was removed from the exclusions list.
    const proxyExclusionsChangeEvent =
        eventToPromise('proxy-exclusions-change', proxyExclusions);
    clearBtn.click();
    await proxyExclusionsChangeEvent;
    await flushTasks();
    assertDeepEquals(1, 1);
    assertDeepEquals(proxyExclusions.exclusions, ['two', 'three']);
  });
});
