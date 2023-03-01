// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/os_settings.js';

import {setHotspotConfigForTesting} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.js';
import {HotspotAllowStatus, HotspotState} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.mojom-webui.js';
import {FakeHotspotConfig} from 'chrome://resources/ash/common/hotspot/fake_hotspot_config.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

suite('NetworkSummary', function() {
  /** @type {!NetworkSummaryElement|undefined} */
  let netSummary;

  /** @type {?CrosHotspotConfigInterface} */
  let hotspotConfig_ = null;

  suiteSetup(function() {
    hotspotConfig_ = new FakeHotspotConfig();
    setHotspotConfigForTesting(hotspotConfig_);
  });

  setup(function() {
    netSummary = document.createElement('network-summary');
    document.body.appendChild(netSummary);
    flush();
  });

  function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  test('Default network summary item', function() {
    const summaryItems =
        netSummary.shadowRoot.querySelectorAll('network-summary-item');
    assertEquals(1, summaryItems.length);
    assertEquals('WiFi', summaryItems[0].id);
  });

  [false, true].forEach(isHotspotFeatureEnabled => {
    test(
        `Hotspot summary item when feature enabled is: ${
            isHotspotFeatureEnabled}`,
        async () => {
          loadTimeData.overrideValues(
              {'isHotspotEnabled': isHotspotFeatureEnabled});
          if (isHotspotFeatureEnabled) {
            hotspotConfig_.setFakeHotspotInfo({
              state: HotspotState.kDisabled,
              allowStatus: HotspotAllowStatus.kDisallowedNoCellularUpstream,
              clientCount: 0,
              config: {
                ssid: 'test_ssid',
                passphrase: 'test_passphrase',
              },
            });
          }
          netSummary = document.createElement('network-summary');
          document.body.appendChild(netSummary);
          await flushAsync();

          let hotspotSummaryItem =
              netSummary.shadowRoot.querySelector('hotspot-summary-item');
          if (isHotspotFeatureEnabled) {
            // kDisallowedNoCellularUpstream or kDisallowedNoWiFiDownstream
            // allow status should hide the hotspot summary.
            assertEquals(null, hotspotSummaryItem);

            hotspotConfig_.setFakeHotspotAllowStatus(
                HotspotAllowStatus.kDisallowedNoWiFiDownstream);
            await flushAsync();
            hotspotSummaryItem =
                netSummary.shadowRoot.querySelector('hotspot-summary-item');
            assertEquals(null, hotspotSummaryItem);

            // Simulate allow status to kAllowed and should show hotspot summary
            hotspotConfig_.setFakeHotspotAllowStatus(
                HotspotAllowStatus.kAllowed);
            await flushAsync();

            hotspotSummaryItem =
                netSummary.shadowRoot.querySelector('hotspot-summary-item');
            assertTrue(!!hotspotSummaryItem);

          } else {
            assertEquals(null, hotspotSummaryItem);
          }
        });
  });

});
