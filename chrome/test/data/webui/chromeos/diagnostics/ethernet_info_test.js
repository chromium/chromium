// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/ethernet_info.js';
import {fakeEthernetNetwork} from 'chrome://diagnostics/fake_data.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, isVisible} from '../../test_util.m.js';

import {assertTextContains, getDataPointValue} from './diagnostics_test_utils.js';

export function ethernetInfoTestSuite() {
  /** @type {?EthernetInfoElement} */
  let ethernetInfoElement = null;

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    ethernetInfoElement.remove();
    ethernetInfoElement = null;
  });

  function initializeEthernetInfo() {
    assertFalse(!!ethernetInfoElement);

    // Add the ethernet info to the DOM.
    ethernetInfoElement =
        /** @type {!EthernetInfoElement} */ (
            document.createElement('ethernet-info'));
    assertTrue(!!ethernetInfoElement);
    ethernetInfoElement.network = fakeEthernetNetwork;
    document.body.appendChild(ethernetInfoElement);

    return flushTasks();
  }

  test('EthernetInfoPopulated', () => {
    return initializeEthernetInfo().then(() => {
      // Element expected on screen but data currently missing in api.
      // TODO(ashleydp): Update test when link speed data-point value provided.
      assertTextContains(
          getDataPointValue(ethernetInfoElement, '#linkSpeed'), '');
    });
  });
}
