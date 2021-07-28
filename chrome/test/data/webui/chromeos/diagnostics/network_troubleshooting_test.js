// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NetworkTroubleshootingElement} from 'chrome://diagnostics/network_troubleshooting.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

export function networkTroubleshootingTestSuite() {
  /** @type {?NetworkTroubleshootingElement} */
  let networkTroubleshootingElement = null;

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    networkTroubleshootingElement.remove();
    networkTroubleshootingElement = null;
  });

  /**
   * @param {string} type
   * @return {!Promise}
   */
  function initializeNetworkTroubleshooting(type) {
    assertFalse(!!networkTroubleshootingElement);

    // Add the network troubleshooting element to the DOM.
    networkTroubleshootingElement =
        /** @type {!NetworkTroubleshootingElement} */ (
            document.createElement('network-troubleshooting'));
    assertTrue(!!networkTroubleshootingElement);
    networkTroubleshootingElement.networkType = type;
    document.body.appendChild(networkTroubleshootingElement);

    return flushTasks();
  }

  test('CorrectNetworkTypeDisplayedInMessage', () => {
    return initializeNetworkTroubleshooting(loadTimeData.getString('wifiLabel'))
        .then(() => {
          dx_utils.assertElementContainsText(
              networkTroubleshootingElement.shadowRoot.querySelector(
                  '#troubleConnectingText'),
              loadTimeData.getString('wifiLabel'));
        });
  });
}
