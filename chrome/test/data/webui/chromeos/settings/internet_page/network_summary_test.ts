// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {NetworkSummaryElement} from 'chrome://os-settings/os_settings.js';
import {setHotspotConfigForTesting} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.js';
import {HotspotAllowStatus, HotspotInfo, HotspotState} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.mojom-webui.js';
import {FakeHotspotConfig} from 'chrome://resources/ash/common/hotspot/fake_hotspot_config.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {NetworkStateProperties} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('NetworkSummary', () => {
  let netSummary: NetworkSummaryElement;
  let hotspotConfig: FakeHotspotConfig;
  let mojoApi: FakeNetworkConfig;

  suiteSetup(() => {
    hotspotConfig = new FakeHotspotConfig();
    mojoApi = new FakeNetworkConfig();
    setHotspotConfigForTesting(hotspotConfig);
    MojoInterfaceProviderImpl.getInstance().setMojoServiceRemoteForTest(
        mojoApi);
  });

  setup(() => {
    netSummary = document.createElement('network-summary');
    document.body.appendChild(netSummary);
    flush();
  });

  teardown(() => {
    netSummary.remove();
  });

  function setNetworksForTest(
      type: NetworkType, networks: NetworkStateProperties[]): void {
    // mojoApi.resetForTest();
    mojoApi.setNetworkTypeEnabledState(type, true);
    mojoApi.addNetworksForTest(networks);
  }

  test('Default network summary item', () => {
    const summaryItems =
        netSummary.shadowRoot!.querySelectorAll('network-summary-item');
    assertEquals(1, summaryItems.length);
    assertEquals('WiFi', summaryItems[0]!.id);
  });


  test('Hotspot summary item', async () => {
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

    netSummary = document.createElement('network-summary');
    document.body.appendChild(netSummary);
    await flushTasks();

    let hotspotSummaryItem =
        netSummary.shadowRoot!.querySelector('hotspot-summary-item');

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
    hotspotConfig.setFakeHotspotAllowStatus(HotspotAllowStatus.kAllowed);
    await flushTasks();

    hotspotSummaryItem =
        netSummary.shadowRoot!.querySelector('hotspot-summary-item');
    assert(hotspotSummaryItem);
  });

  [false, true].forEach(isInstantHotspotRebrandEnabled => {
    test(
        `Tether hosts section is shown?: ${isInstantHotspotRebrandEnabled}`,
        async () => {
          loadTimeData.overrideValues({
            'isInstantHotspotRebrandEnabled': isInstantHotspotRebrandEnabled,
          });
          setNetworksForTest(NetworkType.kTether, [
            OncMojo.getDefaultNetworkState(NetworkType.kTether, 'tether1'),
          ]);
          setNetworksForTest(NetworkType.kCellular, [
            OncMojo.getDefaultNetworkState(NetworkType.kCellular, 'cellular1'),
          ]);
          await flushTasks();

          netSummary = document.createElement('network-summary');
          document.body.appendChild(netSummary);
          await flushTasks();

          const summaryItems =
              netSummary.shadowRoot!.querySelectorAll('network-summary-item');

          // If flag is enabled, tether hosts will be categorized separately
          // from cellular devices.
          if (isInstantHotspotRebrandEnabled) {
            assertEquals(3, summaryItems.length);
            assertEquals('Ethernet', summaryItems[0]!.id);
            assertEquals('Cellular', summaryItems[1]!.id);
            assertEquals('Tether', summaryItems[2]!.id);
          } else {
            assertEquals(2, summaryItems.length);
            assertEquals('Ethernet', summaryItems[0]!.id);
            assertEquals('Cellular', summaryItems[1]!.id);
          }
        });
  });

});
