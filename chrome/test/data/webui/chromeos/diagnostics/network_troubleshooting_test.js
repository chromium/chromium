// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TroubleshootingInfo} from 'chrome://diagnostics/diagnostics_types.js';
import {NetworkTroubleshootingElement} from 'chrome://diagnostics/network_troubleshooting.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

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
   * @param {!TroubleshootingInfo} info
   * @return {!Promise}
   */
  function initializeNetworkTroubleshooting(info) {
    assertFalse(!!networkTroubleshootingElement);

    // Add the network troubleshooting element to the DOM.
    networkTroubleshootingElement =
        /** @type {!NetworkTroubleshootingElement} */ (
            document.createElement('network-troubleshooting'));
    assertTrue(!!networkTroubleshootingElement);
    networkTroubleshootingElement.troubleshootingInfo = info;
    document.body.appendChild(networkTroubleshootingElement);

    return flushTasks();
  }

  test('CorrectInfoDisplayedInTroubleshootingElement', () => {
    return initializeNetworkTroubleshooting({
             header: 'header',
             linkText: 'linkText',
             url: 'https://google.com',
           })
        .then(() => {
          dx_utils.assertElementContainsText(
              networkTroubleshootingElement.shadowRoot.querySelector(
                  '#troubleshootingText'),
              'header');
          dx_utils.assertElementContainsText(
              networkTroubleshootingElement.shadowRoot.querySelector(
                  '#troubleshootingLinkText'),
              'linkText');
        });
  });
}
