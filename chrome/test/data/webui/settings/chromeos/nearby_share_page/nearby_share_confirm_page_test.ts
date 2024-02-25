// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {NearbyShareConfirmPageElement, nearbyShareMojom} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

const {TransferStatus} = nearbyShareMojom;

suite('<nearby-share-confirm-page>', () => {
  let nearbyShareConfirmPage: NearbyShareConfirmPageElement;

  setup(() => {
    nearbyShareConfirmPage =
        document.createElement('nearby-share-confirm-page');

    document.body.appendChild(nearbyShareConfirmPage);
    flush();
  });

  teardown(() => {
    nearbyShareConfirmPage.remove();
  });

  test('renders progress bar', async () => {
    nearbyShareConfirmPage.set('transferStatus', TransferStatus['kConnecting']);
    await flushTasks();

    const isAnimationHidden =
        !!nearbyShareConfirmPage.shadowRoot!.querySelector('cr-lottie[style]');

    if (nearbyShareConfirmPage.shadowRoot!.querySelector('#errorTitle')) {
      assertTrue(isAnimationHidden);
    } else {
      assertFalse(isAnimationHidden);
    }
  });

  test('hide progress bar when error', async () => {
    nearbyShareConfirmPage.set('transferStatus', TransferStatus['kRejected']);
    await flushTasks();

    const isAnimationHidden =
        !!nearbyShareConfirmPage.shadowRoot!.querySelector('cr-lottie[style]');

    if (nearbyShareConfirmPage.shadowRoot!.querySelector('#errorTitle')) {
      assertTrue(isAnimationHidden);
    } else {
      assertFalse(isAnimationHidden);
    }
  });

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
      MAX_VALUE: true,
    };

    // TypeScript augments numerical enums with additional keys (reverse
    // mappings), so need to filter those out when iterating over enum keys.
    const keys =
        (Object.keys(TransferStatus) as Array<keyof typeof TransferStatus>)
            .filter(k => Number.isInteger(TransferStatus[k]));
    for (const key of keys) {
      if (!nonErrorStates.hasOwnProperty(key)) {
        nearbyShareConfirmPage.set(
            'transferStatus',
            TransferStatus[key as keyof typeof TransferStatus]);
        await flushTasks();
        assert(
            nearbyShareConfirmPage.shadowRoot!.querySelector(
                                                  '#errorTitle')!.textContent);

        // Set back to a good state
        nearbyShareConfirmPage.set(
            'transferStatus', TransferStatus['kConnecting']);
        await flushTasks();
        assertEquals(
            null,
            nearbyShareConfirmPage.shadowRoot!.querySelector('#errorTitle'));
      }
    }
  });
});
