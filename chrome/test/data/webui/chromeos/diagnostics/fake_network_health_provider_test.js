// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NetworkListObserver} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeNetworkGuidInfoList} from 'chrome://diagnostics/fake_data.js';
import {FakeNetworkHealthProvider} from 'chrome://diagnostics/fake_network_health_provider.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';

import {assertDeepEquals, assertTrue} from '../../chai_assert.js';

export function fakeNetworkHealthProviderTestSuite() {
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
    let firstResolver = new PromiseResolver();
    let completeResolver = new PromiseResolver();

    /** @type {!NetworkListObserver} */
    const networkListObserverRemote = /** @type {!NetworkListObserver} */ ({
      onNetworkListChanged: (networkGuids) => {
        // Only expect 2 calls.
        assertTrue(whichSample >= 0);
        assertTrue(whichSample <= 1);
        assertDeepEquals(fakeNetworkGuidInfoList[whichSample], networkGuids);

        if (whichSample === 0) {
          firstResolver.resolve();
        } else {
          completeResolver.resolve();
        }
        whichSample++;
      }
    });

    return provider.observeNetworkList(networkListObserverRemote)
        .then(() => firstResolver.promise)
        .then(() => {
          // After the observer fires the first time, trigger it to
          // fire again.
          provider.triggerNetworkListObserver();
          return completeResolver.promise;
        });
  });
}
