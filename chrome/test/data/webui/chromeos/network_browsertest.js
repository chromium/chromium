// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the Network debug UI.
 */

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "content/public/test/browser_test.h"');

function NetworkDebugUIBrowserTest() {}

var NetworkDebugUIBrowserTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return 'chrome://network';
  }

  /** @override */
  get isAsync() {
    return true;
  }
  /** @override */
  get featureList() {
    return {enabled: ['ash::features::kWifiDirect']};
    ;
  }

  get extraLibraries() {
    return [
      '//third_party/node/node_modules/mocha/mocha.js',
      '//chrome/test/data/webui/mocha_adapter.js',
    ];
  }
};

TEST_F('NetworkDebugUIBrowserTest', 'NetworkDebugUI_TabNames', function() {
  test('checks the title of all tabs', async function() {
    const {assertEquals} = await import('chrome://webui-test/chai_assert.js');
    const tabs = document.querySelector('network-ui')
                     .shadowRoot.querySelector('cr-tabs')
                     .shadowRoot.querySelectorAll('.tab')

    assertEquals('General', tabs[0].textContent.trim());
    assertEquals('Network Health', tabs[1].textContent.trim());
    assertEquals('Network Logs', tabs[2].textContent.trim());
    assertEquals('Network State', tabs[3].textContent.trim());
    assertEquals('Network Select', tabs[4].textContent.trim());
    assertEquals('Traffic Counters', tabs[5].textContent.trim());
    assertEquals('Network Metrics', tabs[6].textContent.trim());
    assertEquals('Hotspot', tabs[7].textContent.trim());
    assertEquals('WiFi Direct', tabs[8].textContent.trim());
  });

  mocha.run();
});

TEST_F('NetworkDebugUIBrowserTest', 'NetworkDebugUI_General', function() {
  test('check few items in the General tab', async function() {
    const {assertEquals} = await import('chrome://webui-test/chai_assert.js');
    const root = document.querySelector('network-ui').shadowRoot

    assertEquals(
        'Open Cellular Activation UI',
        root.querySelector('#cellular-activation-button').textContent.trim())
  });

  mocha.run();
});

TEST_F('NetworkDebugUIBrowserTest', 'NetworkDebugUI_Health', function() {
  test('check few items in the health tab', async function() {
    const {assertEquals} = await import('chrome://webui-test/chai_assert.js');
    const root = document.querySelector('network-ui').shadowRoot

    const tabs =
        root.querySelector('cr-tabs').shadowRoot.querySelectorAll('.tab')

    const healthTab = tabs[1]
    healthTab.click()

    const headers = root.querySelector('#health').querySelectorAll('h2')

    assertEquals('Network Health Snapshot', headers[0].textContent)
    assertEquals('Network Diagnostic Routines', headers[1].textContent)
  });

  mocha.run();
});

TEST_F('NetworkDebugUIBrowserTest', 'NetworkDebugUI_Logs', function() {
  test('check few items in the Logs tab', async function() {
    const {assertEquals} = await import('chrome://webui-test/chai_assert.js');
    const root = document.querySelector('network-ui').shadowRoot

    const tabs =
        root.querySelector('cr-tabs').shadowRoot.querySelectorAll('.tab')

    const logsTab = tabs[2]
    logsTab.click()

    const headers = root.querySelector('#logs')
                        .querySelector('network-logs-ui')
                        .shadowRoot.querySelectorAll('h2')

    assertEquals('Network Logs', headers[0].textContent)
  });

  mocha.run();
});

TEST_F('NetworkDebugUIBrowserTest', 'NetworkDebugUI_State', function() {
  test('check few items in the State tab', async function() {
    const {assertEquals} = await import('chrome://webui-test/chai_assert.js');
    const root = document.querySelector('network-ui').shadowRoot

    const tabs =
        root.querySelector('cr-tabs').shadowRoot.querySelectorAll('.tab')

    const stateTab = tabs[3]
    stateTab.click()

    const refresh_button = root.querySelector('#state')
                               .querySelector('network-state-ui')
                               .shadowRoot.querySelector('#refresh')

    assertEquals('Refresh Networks', refresh_button.textContent.trim())
  });

  mocha.run();
});

TEST_F('NetworkDebugUIBrowserTest', 'NetworkDebugUI_Counters', function() {
  test('check few items in the Counters tab', async function() {
    const {assertEquals} = await import('chrome://webui-test/chai_assert.js');
    const root = document.querySelector('network-ui').shadowRoot

    const tabs =
        root.querySelector('cr-tabs').shadowRoot.querySelectorAll('.tab')

    const countersTab = tabs[5]
    countersTab.click()

    const request_button = root.querySelector('#counters')
                               .querySelector('traffic-counters')
                               .shadowRoot.querySelector('#requestButton')

    assertEquals('Request Traffic Counters', request_button.textContent.trim())
  });

  mocha.run();
});

TEST_F('NetworkDebugUIBrowserTest', 'NetworkDebugUI_Metrics', function() {
  test('check few items in the Metrics tab', async function() {
    const {assertEquals} = await import('chrome://webui-test/chai_assert.js');
    const root = document.querySelector('network-ui').shadowRoot

    const tabs =
        root.querySelector('cr-tabs').shadowRoot.querySelectorAll('.tab')

    const metricsTab = tabs[6]
    metricsTab.click()

    const buttons = root.querySelector('#metrics')
                        .querySelector('network-metrics-ui')
                        .shadowRoot.querySelectorAll('cr-button')

    assertEquals('Render', buttons[0].textContent.trim())
    assertEquals('Start', buttons[1].textContent.trim())
    assertEquals('Stop', buttons[2].textContent.trim())
    assertEquals('Increase rate', buttons[3].textContent.trim())
    assertEquals('Decrease rate', buttons[4].textContent.trim())
  });

  mocha.run();
});

TEST_F('NetworkDebugUIBrowserTest', 'NetworkDebugUI_Hotspot', function() {
  test('check few items in the Hotspot tab', async function() {
    const {assertEquals} = await import('chrome://webui-test/chai_assert.js');
    const root = document.querySelector('network-ui').shadowRoot

    const tabs =
        root.querySelector('cr-tabs').shadowRoot.querySelectorAll('.tab')

    const hotspotTab = tabs[7]
    hotspotTab.click()

    const headers = root.querySelector('#hotspot').querySelectorAll('h2')

    assertEquals('Tethering Capabilities:', headers[0].textContent)
    assertEquals('Tethering Status:', headers[1].textContent)
    assertEquals('Tethering Configuration:', headers[2].textContent)
  });

  mocha.run();
});
