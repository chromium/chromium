// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';
// #import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('NearbyShare', function() {
  let nearbyShareConfirmPage;

  setup(function() {
    PolymerTest.clearBody();

    nearbyShareConfirmPage =
        document.createElement('nearby-share-confirm-page');

    document.body.appendChild(nearbyShareConfirmPage);
    Polymer.dom.flush();
  });

  async function flushAsync() {
    Polymer.dom.flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  test('Ensure all final transfer states explicitly handled', async () => {
    // Transfer states that do not result in an error message.
    const nonErrorStates = {
      kUnknown: true,
      kConnecting: true,
      kAwaitingLocalConfirmation: true,
      kAwaitingRemoteAcceptance: true,
      kInProgress: true,
      kMediaDownloading: true,
      kComplete: true,
      kRejected: true,
      MIN_VALUE: true,
      MAX_VALUE: true
    };

    let key;
    for (key of Object.keys(nearbyShare.mojom.TransferStatus)) {
      const isErrorState = !(key in nonErrorStates);
      if (isErrorState) {
        nearbyShareConfirmPage.set(
            'transferStatus', nearbyShare.mojom.TransferStatus[key]);
        await flushAsync();
        assertTrue(!!nearbyShareConfirmPage.$$('#errorTitle').textContent);

        // Set back to a good state
        nearbyShareConfirmPage.set(
            'transferStatus', nearbyShare.mojom.TransferStatus['kConnecting']);
        await flushAsync();
        assertFalse(!!nearbyShareConfirmPage.$$('#errorTitle'));
      }
    }
  });
});