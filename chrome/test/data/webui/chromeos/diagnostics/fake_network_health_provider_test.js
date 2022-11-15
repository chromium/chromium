// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {fakeCellularNetwork, fakeEthernetNetwork, fakeNetworkGuidInfoList, fakeWifiNetwork} from 'chrome://diagnostics/fake_data.js';
import {FakeNetworkHealthProvider} from 'chrome://diagnostics/fake_network_health_provider.js';
import {NetworkListObserverRemote, NetworkStateObserverRemote} from 'chrome://diagnostics/network_health_provider.mojom-webui.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';

import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('fakeNetworkHealthProviderTestSuite', function() {
  /** @type {?FakeNetworkHealthProvider} */
  let provider = null;

  setup(() => {
    provider = new FakeNetworkHealthProvider();
  });

  teardown(() => {
    provider = null;
  });

  test('ObserveNetworkListTwiceWithTrigger', () => {
    // The fake needs to have at least 2 samples.
    assertTrue(fakeNetworkGuidInfoList.length >= 2);
    provider.setFakeNetworkGuidInfo(fakeNetworkGuidInfoList);

    // Keep track of which observation we should get.
    let whichSample = 0;
    const firstResolver = new PromiseResolver();
    const completeResolver = new PromiseResolver();

    const networkListObserverRemote =
        /** @type {!NetworkListObserverRemote} */ ({
          onNetworkListChanged: (networkGuids, activeGuid) => {
            // Only expect 2 calls.
            assertTrue(whichSample >= 0);
            assertTrue(whichSample <= 1);
            assertDeepEquals(
                fakeNetworkGuidInfoList[whichSample].networkGuids,
                networkGuids);
            assertEquals(
                fakeNetworkGuidInfoList[whichSample].activeGuid, activeGuid);

            if (whichSample === 0) {
              firstResolver.resolve();
            } else {
              completeResolver.resolve();
            }
            whichSample++;
          },
        });

    provider.observeNetworkList(networkListObserverRemote);
    return provider.getObserveNetworkListPromiseForTesting()
        .then(() => firstResolver.promise)
        .then(() => {
          // After the observer fires the first time, trigger it to
          // fire again.
          provider.triggerNetworkListObserver();
          return completeResolver.promise;
        });
  });

  test('ObserveNetwork', () => {
    provider.setFakeNetworkGuidInfo(fakeNetworkGuidInfoList);
    provider.setFakeNetworkState('cellularGuid', [fakeCellularNetwork]);
    const resolver = new PromiseResolver();

    const networkStateObserverRemote =
        /** @type {!NetworkStateObserverRemote} */ ({
          onNetworkStateChanged: (network) => {
            assertDeepEquals(fakeCellularNetwork, network);
            resolver.resolve();
          },
        });

    provider.observeNetwork(networkStateObserverRemote, 'cellularGuid');
    return provider.getObserveNetworkStatePromiseForTesting().then(
        () => resolver.promise);
  });

  test('ObserveNetworkSupportsMultipleObservers', () => {
    provider.setFakeNetworkGuidInfo(fakeNetworkGuidInfoList);
    provider.setFakeNetworkState('wifiGuid', [fakeWifiNetwork]);
    provider.setFakeNetworkState('ethernetGuid', [fakeEthernetNetwork]);
    const wifiResolver = new PromiseResolver();
    const ethernetResolver = new PromiseResolver();

    const wifiNetworkStateObserverRemote =
        /** @type {!NetworkStateObserverRemote} */ ({
          onNetworkStateChanged: (network) => {
            assertDeepEquals(fakeWifiNetwork, network);
            wifiResolver.resolve();
          },
        });

    const ethernetNetworkStateObserverRemote =
        /** @type {!NetworkStateObserverRemote} */ ({
          onNetworkStateChanged: (network) => {
            assertDeepEquals(fakeEthernetNetwork, network);
            ethernetResolver.resolve();
          },
        });

    provider.observeNetwork(wifiNetworkStateObserverRemote, 'wifiGuid');
    return provider.getObserveNetworkStatePromiseForTesting()
        .then(() => wifiResolver.promise)
        .then(
            () => provider.observeNetwork(
                ethernetNetworkStateObserverRemote, 'ethernetGuid'))
        .then(() => ethernetResolver.promise);
  });
});
