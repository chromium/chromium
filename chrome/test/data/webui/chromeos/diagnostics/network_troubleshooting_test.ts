// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/strings.m.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';
import 'chrome://diagnostics/network_troubleshooting.js';

import {TroubleshootingInfo} from 'chrome://diagnostics/diagnostics_types.js';
import {NetworkTroubleshootingElement} from 'chrome://diagnostics/network_troubleshooting.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertFalse} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';

suite('networkTroubleshootingTestSuite', function() {
  let networkTroubleshootingElement: NetworkTroubleshootingElement|null = null;

  const troubleShootingInfo = {
    header: 'header',
    linkText: 'linkText',
    url: 'https://google.com',
  };

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  teardown(() => {
    networkTroubleshootingElement?.remove();
    networkTroubleshootingElement = null;
  });

  function getLinkTextElement(): HTMLElement {
    assert(networkTroubleshootingElement);

    return networkTroubleshootingElement.shadowRoot!.querySelector(
               '#troubleshootingLinkText') as HTMLElement;
  }

  function initializeNetworkTroubleshooting(info: TroubleshootingInfo):
      Promise<void> {
    // Add the network troubleshooting element to the DOM.
    networkTroubleshootingElement =
        document.createElement('network-troubleshooting');
    assert(networkTroubleshootingElement);
    networkTroubleshootingElement.troubleshootingInfo = info;
    document.body.appendChild(networkTroubleshootingElement);

    return flushTasks();
  }

  test('CorrectInfoDisplayedInTroubleshootingElement', async () => {
    await initializeNetworkTroubleshooting(troubleShootingInfo);
    assert(networkTroubleshootingElement);
    dx_utils.assertElementContainsText(
        networkTroubleshootingElement.shadowRoot!.querySelector(
            '#troubleshootingText'),
        'header');
    dx_utils.assertElementContainsText(
        networkTroubleshootingElement.shadowRoot!.querySelector(
            '#troubleshootingLinkText'),
        'linkText');
  });

  test('IsLoggedInFalseThenLinkTextHidden', async () => {
    loadTimeData.overrideValues({isLoggedIn: false});
    await initializeNetworkTroubleshooting(troubleShootingInfo);
    assert(networkTroubleshootingElement);
    dx_utils.assertElementContainsText(
        networkTroubleshootingElement.shadowRoot!.querySelector(
            '#troubleshootingText'),
        'header');
    assertFalse(isVisible(getLinkTextElement()));
  });
});
