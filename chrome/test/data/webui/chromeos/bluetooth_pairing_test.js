// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://bluetooth-pairing/bluetooth_pairing_dialog.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

suite('bluetooth-pairing-dialog', () => {
  let bluetoothPairingDialog = null;

  function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  setup(() => {
    PolymerTest.clearBody();
  });

  async function init() {
    bluetoothPairingDialog = document.createElement('bluetooth-pairing-dialog');
    document.body.appendChild(bluetoothPairingDialog);
    await flushAsync();
  }

  [false, true].forEach(isJellyEnabled => {
    test(
        `CSS theme is updated when isJellyEnabled is ${isJellyEnabled}`,
        async () => {
          loadTimeData.overrideValues({
            isJellyEnabled: isJellyEnabled,
          });
          await init();

          const link = document.head.querySelector(
              `link[href*='chrome://theme/colors.css']`);
          if (isJellyEnabled) {
            assertTrue(!!link);
            assertTrue(document.body.classList.contains('jelly-enabled'));
          } else {
            assertEquals(null, link);
            assertFalse(document.body.classList.contains('jelly-enabled'));
          }
        });
  });
});
