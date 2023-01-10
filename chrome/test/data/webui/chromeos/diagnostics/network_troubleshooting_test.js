// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/strings.m.js';
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {NetworkTroubleshootingElement} from 'chrome://diagnostics/network_troubleshooting.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {isVisible} from '../test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';

suite('networkTroubleshootingTestSuite', function() {
  /** @type {?NetworkTroubleshootingElement} */
  let networkTroubleshootingElement = null;

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    networkTroubleshootingElement.remove();
    networkTroubleshootingElement = null;
  });

  /** @return {!HTMLElement} */
  function getLinkTextElement() {
    assertTrue(!!networkTroubleshootingElement);

    return /** @type {!HTMLElement} */ (
        networkTroubleshootingElement.shadowRoot.querySelector(
            '#troubleshootingLinkText'));
  }

  /**
   * @suppress {visibility}
   * @param {boolean} state
   * @return {!Promise}
   */
  function setIsLoggedIn(state) {
    assertTrue(!!networkTroubleshootingElement);
    networkTroubleshootingElement.isLoggedIn = state;

    return flushTasks();
  }

  /** @return {!Promise} */
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

  test('IsLoggedInFalseThenLinkTextHidden', () => {
    const troubleShootingInfo = {
      header: 'header',
      linkText: 'linkText',
      url: 'https://google.com',
    };
    return initializeNetworkTroubleshooting(troubleShootingInfo)
        .then(() => setIsLoggedIn(false))
        .then(() => {
          dx_utils.assertElementContainsText(
              networkTroubleshootingElement.shadowRoot.querySelector(
                  '#troubleshootingText'),
              'header');
          assertFalse(isVisible(getLinkTextElement()));
        });
  });
});
