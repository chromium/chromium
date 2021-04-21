// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// So that mojo is defined.
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://nearby/nearby_confirmation_page.js';

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
    return confirmationPageElement.$$('nearby-page-template').$$(button);
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
        nearbyShare.mojom.TransferStatus.kAwaitingLocalConfirmation,
        /*token=*/ null);
    await transferUpdateListener.remote_.$.flushForTesting();

    getButton('#actionButton').click();
    await confirmationManager.whenCalled('accept');
  });

  test('calls reject on click', async function() {
    transferUpdateListener.remote_.onTransferUpdate(
        nearbyShare.mojom.TransferStatus.kAwaitingLocalConfirmation,
        /*token=*/ null);
    await transferUpdateListener.remote_.$.flushForTesting();

    getButton('#cancelButton').click();
    await confirmationManager.whenCalled('reject');
  });

  test('calls cancel on click', async function() {
    transferUpdateListener.remote_.onTransferUpdate(
        nearbyShare.mojom.TransferStatus.kAwaitingRemoteAcceptance,
        /*token=*/ null);
    await transferUpdateListener.remote_.$.flushForTesting();

    getButton('#cancelButton').click();
    await confirmationManager.whenCalled('cancel');
  });

  test('renders confirmation token', async function() {
    const token = 'TestToken1234';
    transferUpdateListener.remote_.onTransferUpdate(
        nearbyShare.mojom.TransferStatus.kAwaitingLocalConfirmation, token);
    await transferUpdateListener.remote_.$.flushForTesting();

    const renderedToken =
        confirmationPageElement.$$('#confirmationToken').textContent;
    assertTrue(renderedToken.includes(token));
  });

  test('renders share target name', function() {
    const name = 'Device Name';
    confirmationPageElement.shareTarget =
        /** @type {!nearbyShare.mojom.ShareTarget} */ ({
          id: {high: BigInt(0), low: BigInt(0)},
          name,
          type: nearbyShare.mojom.ShareTargetType.kPhone,
          payloadPreview: null,
        });
    const renderedName = confirmationPageElement.$$('nearby-progress')
                             .$$('#device-name')
                             .innerText;
    assertEquals(name, renderedName);
  });

  test('renders attachment title', function() {
    const title = 'Filename';
    confirmationPageElement.payloadPreview = {
      description: title,
      fileCount: 1,
      shareType: nearbyShare.mojom.ShareType.kUnknownFile
    };
    const renderedTitle =
        confirmationPageElement.$$('nearby-preview').$$('#title').textContent;
    assertEquals(title, renderedTitle);
  });

  test('renders error', async function() {
    const token = 'TestToken1234';
    transferUpdateListener.remote_.onTransferUpdate(
        nearbyShare.mojom.TransferStatus.kRejected, token);
    await transferUpdateListener.remote_.$.flushForTesting();

    const errorTitle = confirmationPageElement.$$('#errorTitle').textContent;
    assertTrue(!!errorTitle);
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
      MAX_VALUE: true
    };

    let key;
    for (key of Object.keys(nearbyShare.mojom.TransferStatus)) {
      const isErrorState = !(key in nonErrorStates);
      const token = 'TestToken1234';
      if (isErrorState) {
        transferUpdateListener.remote_.onTransferUpdate(
            nearbyShare.mojom.TransferStatus[key], token);
        await transferUpdateListener.remote_.$.flushForTesting();

        assertTrue(!!confirmationPageElement.$$('#errorTitle').textContent);

        // Set back to a good state
        confirmationPageElement.set('errorTitle_', null);
        confirmationPageElement.set('errorDescription_', null);
        transferUpdateListener.remote_.onTransferUpdate(
            nearbyShare.mojom.TransferStatus.kConnecting, token);
        await transferUpdateListener.remote_.$.flushForTesting();
        assertFalse(!!confirmationPageElement.$$('#errorTitle'));
      }
    }
  });

  test('gets transfer info for testing', async function() {
    const token = 'TestToken1234';
    transferUpdateListener.remote_.onTransferUpdate(
        nearbyShare.mojom.TransferStatus.kRejected, token);
    await transferUpdateListener.remote_.$.flushForTesting();

    const info = confirmationPageElement.getTransferInfoForTesting();
    assertEquals(
        info.transferStatus, nearbyShare.mojom.TransferStatus.kRejected);
    assertEquals(info.confirmationToken, token);
    assertTrue(!!info.errorTitle);
    assertTrue(!!info.errorDescription);
  });
});
