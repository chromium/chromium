// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {NetworkSummaryElement} from 'chrome://os-settings/os_settings.js';
import {setHotspotConfigForTesting} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.js';
import {HotspotAllowStatus, HotspotInfo, HotspotState} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.mojom-webui.js';
import {FakeHotspotConfig} from 'chrome://resources/ash/common/hotspot/fake_hotspot_config.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('NetworkSummary', () => {
  let netSummary: NetworkSummaryElement;
  let hotspotConfig: FakeHotspotConfig;

  suiteSetup(() => {
    hotspotConfig = new FakeHotspotConfig();
    setHotspotConfigForTesting(hotspotConfig);
  });

  setup(() => {
    netSummary = document.createElement('network-summary');
    document.body.appendChild(netSummary);
    flush();
  });

  teardown(() => {
    netSummary.remove();
  });

  test('Default network summary item', () => {
    const summaryItems =
        netSummary.shadowRoot!.querySelectorAll('network-summary-item');
    assertEquals(1, summaryItems.length);
    assertEquals('WiFi', summaryItems[0]!.id);
  });

  [false, true].forEach(isHotspotFeatureEnabled => {
    test(
        `Hotspot summary item when feature enabled is: ${
            isHotspotFeatureEnabled}`,
        async () => {
          loadTimeData.overrideValues(
              {'isHotspotEnabled': isHotspotFeatureEnabled});
          if (isHotspotFeatureEnabled) {
            const hotspotInfo = {
              state: HotspotState.kDisabled,
              allowStatus: HotspotAllowStatus.kDisallowedNoCellularUpstream,
              clientCount: 0,
              config: {
                ssid: 'test_ssid',
                passphrase: 'test_passphrase',
              },
            } as HotspotInfo;
            hotspotConfig.setFakeHotspotInfo(hotspotInfo);
          }
          netSummary = document.createElement('network-summary');
          document.body.appendChild(netSummary);
          await flushTasks();

          let hotspotSummaryItem =
              netSummary.shadowRoot!.querySelector('hotspot-summary-item');
          if (isHotspotFeatureEnabled) {
            // kDisallowedNoCellularUpstream or kDisallowedNoWiFiDownstream
            // allow status should hide the hotspot summary.
            assertEquals(null, hotspotSummaryItem);

            hotspotConfig.setFakeHotspotAllowStatus(
                HotspotAllowStatus.kDisallowedNoWiFiDownstream);
            await flushTasks();
            hotspotSummaryItem =
                netSummary.shadowRoot!.querySelector('hotspot-summary-item');
            assertEquals(null, hotspotSummaryItem);

            // Simulate allow status to kAllowed and should show hotspot summary
            hotspotConfig.setFakeHotspotAllowStatus(
                HotspotAllowStatus.kAllowed);
            await flushTasks();

            hotspotSummaryItem =
                netSummary.shadowRoot!.querySelector('hotspot-summary-item');
            assert(hotspotSummaryItem);

          } else {
            assertEquals(null, hotspotSummaryItem);
          }
        });
  });
});
