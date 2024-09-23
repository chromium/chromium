// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeCellularNetwork, fakeEthernetNetwork, fakeNetworkGuidInfoList, fakeWifiNetwork} from 'chrome://diagnostics/fake_data.js';
import {FakeNetworkHealthProvider} from 'chrome://diagnostics/fake_network_health_provider.js';
import {Network, NetworkListObserverRemote, NetworkStateObserverRemote} from 'chrome://diagnostics/network_health_provider.mojom-webui.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('fakeNetworkHealthProviderTestSuite', function() {
  let provider: FakeNetworkHealthProvider|null = null;

  setup(() => {
    provider = new FakeNetworkHealthProvider();
  });

  teardown(() => {
    provider = null;
  });

  test('ObserveNetworkListTwiceWithTrigger', () => {
    assert(provider);
    // The fake needs to have at least 2 samples.
    assertTrue(fakeNetworkGuidInfoList.length >= 2);
    provider.setFakeNetworkGuidInfo(fakeNetworkGuidInfoList);

    // Keep track of which observation we should get.
    let whichSample = 0;
    const firstResolver = new PromiseResolver();
    const completeResolver = new PromiseResolver();

    const networkListObserverRemote = {
      onNetworkListChanged: (networkGuids: string[], activeGuid: string) => {
        // Only expect 2 calls.
        assertTrue(whichSample >= 0);
        assertTrue(whichSample <= 1);
        assertDeepEquals(
            fakeNetworkGuidInfoList[whichSample]!.networkGuids, networkGuids);
        assertEquals(
            fakeNetworkGuidInfoList[whichSample]!.activeGuid, activeGuid);

        if (whichSample === 0) {
          firstResolver.resolve(null);
        } else {
          completeResolver.resolve(null);
        }
        whichSample++;
      },
    };

    provider.observeNetworkList(
        networkListObserverRemote as NetworkListObserverRemote);
    return provider.getObserveNetworkListPromiseForTesting()
        .then(() => firstResolver.promise)
        .then(() => {
          // After the observer fires the first time, trigger it to
          // fire again.
          assert(provider);
          provider.triggerNetworkListObserver();
          return completeResolver.promise;
        });
  });

  test('ObserveNetwork', () => {
    assert(provider);
    provider.setFakeNetworkGuidInfo(fakeNetworkGuidInfoList);
    provider.setFakeNetworkState('cellularGuid', [fakeCellularNetwork]);
    const resolver = new PromiseResolver();

    const networkStateObserverRemote = {
      onNetworkStateChanged: (network: Network) => {
        assertDeepEquals(fakeCellularNetwork, network);
        resolver.resolve(null);
      },
    };

    provider.observeNetwork(
        networkStateObserverRemote as NetworkStateObserverRemote,
        'cellularGuid');
    return provider.getObserveNetworkStatePromiseForTesting().then(
        () => resolver.promise);
  });

  test('ObserveNetworkSupportsMultipleObservers', () => {
    assert(provider);
    provider.setFakeNetworkGuidInfo(fakeNetworkGuidInfoList);
    provider.setFakeNetworkState('wifiGuid', [fakeWifiNetwork]);
    provider.setFakeNetworkState('ethernetGuid', [fakeEthernetNetwork]);
    const wifiResolver = new PromiseResolver();
    const ethernetResolver = new PromiseResolver();

    const wifiNetworkStateObserverRemote = {
      onNetworkStateChanged: (network: Network) => {
        assertDeepEquals(fakeWifiNetwork, network);
        wifiResolver.resolve(null);
      },
    };

    const ethernetNetworkStateObserverRemote = {
      onNetworkStateChanged: (network: Network) => {
        assertDeepEquals(fakeEthernetNetwork, network);
        ethernetResolver.resolve(null);
      },
    };

    provider.observeNetwork(
        wifiNetworkStateObserverRemote as NetworkStateObserverRemote,
        'wifiGuid');
    return provider.getObserveNetworkStatePromiseForTesting()
        .then(() => wifiResolver.promise)
        .then(
            () => provider!.observeNetwork(
                ethernetNetworkStateObserverRemote as
                    NetworkStateObserverRemote,
                'ethernetGuid'))
        .then(() => ethernetResolver.promise);
  });
});
