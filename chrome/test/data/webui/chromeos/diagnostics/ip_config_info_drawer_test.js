// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/ip_config_info_drawer.js';

import {assertTrue} from '../../chai_assert.js';
import {flushTasks, isVisible} from '../../test_util.m.js';

export function ipConfigInfoDrawerTestSuite() {
  /** @type {?IpConfigInfoDrawerElement} */
  let ipConfigInfoDrawerElement = null;

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    ipConfigInfoDrawerElement.remove();
    ipConfigInfoDrawerElement = null;
  });

  function initializeIpConfigInfoDrawerElement() {
    ipConfigInfoDrawerElement = /** @type {!IpConfigInfoDrawerElement} */ (
        document.createElement('ip-config-info-drawer'));
    document.body.appendChild(ipConfigInfoDrawerElement);
    assertTrue(!!ipConfigInfoDrawerElement);
    return flushTasks();
  }

  test('IpConfigInfoDrawerInitialized', () => {
    return initializeIpConfigInfoDrawerElement().then(() => {
      assertTrue(!!ipConfigInfoDrawerElement.$$('#ipConfigInfoElement'));
    });
  });
}
