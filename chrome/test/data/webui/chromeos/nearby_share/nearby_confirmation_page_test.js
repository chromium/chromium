// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://nearby/nearby_confirmation_page.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {TransferStatus} from 'chrome://nearby/shared/nearby_share.mojom-webui.js';
import {ShareType} from 'chrome://nearby/shared/nearby_share_share_type.mojom-webui.js';
import {ShareTargetType} from 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_target_types.mojom-webui.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';

import {FakeConfirmationManagerRemote, FakeTransferUpdateListenerPendingReceiver} from './fake_mojo_interfaces.js';

suite('ConfirmatonPageTest', function() {
  /** @type {!NearbyConfirmationPageElement} */
  let confirmationPageElement;

  /** @type {!FakeConfirmationManagerRemote} */
  let confirmationManager;

  /** @type {!FakeTransferUpdateListenerPendingReceiver} */
  let transferUpdateListener;

  /**
   * @param {string} button button selector (i.e. #actionButton)
   */
  function getButton(button) {
    return confirmationPageElement.shadowRoot
        .querySelector('nearby-page-template')
        .shadowRoot.querySelector(button);
  }

  setup(function() {
    confirmationManager = new FakeConfirmationManagerRemote();
    transferUpdateListener = new FakeTransferUpdateListenerPendingReceiver();
    confirmationPageElement = /** @type {!NearbyConfirmationPageElement} */ (
        document.createElement('nearby-confirmation-page'));
    confirmationPageElement.confirmationManager = confirmationManager;
    confirmationPageElement.transferUpdateListener = transferUpdateListener;
    document.body.appendChild(confirmationPageElement);
  });

  teardown(function() {
    confirmationPageElement.remove();
  });

  test('renders component', function() {
    assertEquals('NEARBY-CONFIRMATION-PAGE', confirmationPageElement.tagName);
  });

  test('calls accept on click', async function() {
    transferUpdateListener.remote_.onTransferUpdate(
        TransferStatus.kAwaitingLocalConfirmation,
        /*token=*/ null);
    await transferUpdateListener.remote_.$.flushForTesting();

    getButton('#actionButton').click();
    await confirmationManager.whenCalled('accept');
  });

  test('calls reject on click', async function() {
    transferUpdateListener.remote_.onTransferUpdate(
        TransferStatus.kAwaitingLocalConfirmation,
        /*token=*/ null);
    await transferUpdateListener.remote_.$.flushForTesting();

    getButton('#cancelButton').click();
    await confirmationManager.whenCalled('reject');
  });

  test('calls cancel on click', async function() {
    transferUpdateListener.remote_.onTransferUpdate(
        TransferStatus.kAwaitingRemoteAcceptance,
        /*token=*/ null);
    await transferUpdateListener.remote_.$.flushForTesting();

    getButton('#cancelButton').click();
    await confirmationManager.whenCalled('cancel');
  });

  test('renders confirmation token', async function() {
    const token = 'TestToken1234';
    transferUpdateListener.remote_.onTransferUpdate(
        TransferStatus.kAwaitingLocalConfirmation, token);
    await transferUpdateListener.remote_.$.flushForTesting();

    const renderedToken =
        confirmationPageElement.shadowRoot.querySelector('#confirmationToken')
            .textContent;
    assertTrue(renderedToken.includes(token));
  });

  test('renders share target name', function() {
    const name = 'Device Name';
    confirmationPageElement.shareTarget =
        /** @type {!ShareTarget} */ ({
          id: {high: BigInt(0), low: BigInt(0)},
          name,
          type: ShareTargetType.kPhone,
          imageUrl: {
            url: 'testImageURL',
          },
          payloadPreview: null,
        });
    const renderedName =
        confirmationPageElement.shadowRoot.querySelector('nearby-progress')
            .shadowRoot.querySelector('#device-name')
            .innerText;
    assertEquals(name, renderedName);
  });

  test('renders attachment title', function() {
    const title = 'Filename';
    confirmationPageElement.payloadPreview = {
      description: title,
      fileCount: 1,
      shareType: ShareType.kUnknownFile,
    };
    const renderedTitle =
        confirmationPageElement.shadowRoot.querySelector('nearby-preview')
            .shadowRoot.querySelector('#title')
            .textContent;
    assertEquals(title, renderedTitle);
  });

  test('renders progress bar', async function() {
    const token = 'TestToken1234';
    transferUpdateListener.remote_.onTransferUpdate(
        TransferStatus.kInProgress, token);
    await transferUpdateListener.remote_.$.flushForTesting();

    const isAnimationShown =
        !!confirmationPageElement.shadowRoot.querySelector('#animation');

    if (confirmationPageElement.shadowRoot.querySelector('#errorTitle')) {
      assertFalse(isAnimationShown);
    } else {
      assertTrue(isAnimationShown);
    }
  });

  test('renders error', async function() {
    const token = 'TestToken1234';
    transferUpdateListener.remote_.onTransferUpdate(
        TransferStatus.kRejected, token);
    await transferUpdateListener.remote_.$.flushForTesting();

    const errorTitle =
        confirmationPageElement.shadowRoot.querySelector('#errorTitle')
            .textContent;
    assertTrue(!!errorTitle);
  });

  test('hide progress bar when error', async function() {
    const token = 'TestToken1234';
    transferUpdateListener.remote_.onTransferUpdate(
        TransferStatus.kRejected, token);
    await transferUpdateListener.remote_.$.flushForTesting();

    const isAnimationShown =
        !!confirmationPageElement.shadowRoot.querySelector('#animation');

    if (confirmationPageElement.shadowRoot.querySelector('#errorTitle')
            .textContent) {
      assertFalse(isAnimationShown);
    } else {
      assertTrue(isAnimationShown);
    }
  });

  test('Ensure all final transfer states explicitly handled', async function() {
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
      kCancelled: true,
      MIN_VALUE: true,
      MAX_VALUE: true,
    };

    // TypeScript augments numerical enums with additional keys (reverse
    // mappings), so need to filter those out when iterating over all keys.
    const keys = Object.keys(TransferStatus)
                     .filter(k => Number.isInteger(TransferStatus[k]));
    for (const key of keys) {
      const isErrorState = !(key in nonErrorStates);
      const token = 'TestToken1234';
      if (isErrorState) {
        transferUpdateListener.remote_.onTransferUpdate(
            TransferStatus[key], token);
        await transferUpdateListener.remote_.$.flushForTesting();

        assertTrue(
            !!confirmationPageElement.shadowRoot.querySelector('#errorTitle')
                  .textContent);

        // Set back to a good state
        confirmationPageElement.set('errorTitle_', null);
        confirmationPageElement.set('errorDescription_', null);
        transferUpdateListener.remote_.onTransferUpdate(
            TransferStatus.kConnecting, token);
        await transferUpdateListener.remote_.$.flushForTesting();
        assertFalse(
            !!confirmationPageElement.shadowRoot.querySelector('#errorTitle'));
      }
    }
  });

  test('gets transfer info for testing', async function() {
    const token = 'TestToken1234';
    transferUpdateListener.remote_.onTransferUpdate(
        TransferStatus.kRejected, token);
    await transferUpdateListener.remote_.$.flushForTesting();

    const info = confirmationPageElement.getTransferInfoForTesting();
    assertEquals(info.transferStatus, TransferStatus.kRejected);
    assertEquals(info.confirmationToken, token);
    assertTrue(!!info.errorTitle);
    assertTrue(!!info.errorDescription);
  });
});
