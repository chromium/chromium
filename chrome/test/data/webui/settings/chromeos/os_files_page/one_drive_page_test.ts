// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {OneDriveConnectionState, SettingsOneDriveSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {CrButtonElement, OneDriveBrowserProxy} from 'chrome://os-settings/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {assertAsync} from '../utils.js';

import {OneDriveTestBrowserProxy, ProxyOptions} from './one_drive_test_browser_proxy.js';

suite('<one-drive-subpage>', function() {
  /* The <one-drive-subpage> page. */
  let oneDrivePage: SettingsOneDriveSubpageElement;
  /* The BrowserProxy element to make assertions on when mojo methods are
     called. */
  let testOneDriveProxy: OneDriveTestBrowserProxy;

  teardown(function() {
    oneDrivePage.remove();
  });

  async function setupOneDrivePage(options: ProxyOptions) {
    testOneDriveProxy = new OneDriveTestBrowserProxy(options);
    OneDriveBrowserProxy.setInstance(testOneDriveProxy);
    oneDrivePage = document.createElement('settings-one-drive-subpage');
    document.body.appendChild(oneDrivePage);
    await oneDrivePage.initPromise;
    flush();
  }

  test('Signed in page content', async () => {
    const email = 'email@gmail.com';
    await setupOneDrivePage({email});
    const signedInAsLabelElement =
        oneDrivePage.shadowRoot!.querySelector<HTMLDivElement>(
            '#signedInAsLabel')!;
    const connectDisconnectButton =
        oneDrivePage.shadowRoot!.querySelector<CrButtonElement>(
            '#oneDriveConnectDisconnect')!;
    const openOneDriveFolderButton =
        oneDrivePage.shadowRoot!.querySelector<CrButtonElement>(
            '#openOneDriveFolder')!;
    assertEquals('Signed in as ' + email, signedInAsLabelElement.innerText);
    assertEquals('Remove access', connectDisconnectButton.textContent!.trim());
    assertFalse(connectDisconnectButton.hasAttribute('disabled'));
    assertTrue(openOneDriveFolderButton.checkVisibility());
  });

  test('Signed out page content', async () => {
    await setupOneDrivePage({
      email: null,
    });
    const signedInAsLabelElement =
        oneDrivePage.shadowRoot!.querySelector<HTMLDivElement>(
            '#signedInAsLabel')!;
    const connectDisconnectButton =
        oneDrivePage.shadowRoot!.querySelector<CrButtonElement>(
            '#oneDriveConnectDisconnect')!;
    const openOneDriveFolderButton =
        oneDrivePage.shadowRoot!.querySelector<CrButtonElement>(
            '#openOneDriveFolder')!;
    assertEquals(
        'Add your Microsoft account', signedInAsLabelElement.innerText);
    assertEquals('Connect', connectDisconnectButton.textContent!.trim());
    assertFalse(connectDisconnectButton.hasAttribute('disabled'));
    assertFalse(!!openOneDriveFolderButton);
  });

  test('Loading state content', async () => {
    // Load the page with a signed in state (connection state "CONNECTED").
    await setupOneDrivePage({
      email: 'email@gmail.com',
    });
    const signedInAsLabelElement =
        oneDrivePage.shadowRoot!.querySelector<HTMLDivElement>(
            '#signedInAsLabel')!;
    const connectDisconnectButton =
        oneDrivePage.shadowRoot!.querySelector<CrButtonElement>(
            '#oneDriveConnectDisconnect')!;
    const openOneDriveFolderButton =
        oneDrivePage.shadowRoot!.querySelector<CrButtonElement>(
            '#openOneDriveFolder')!;
    // Change connection status to "LOADING".
    oneDrivePage.updateConnectionStateForTesting(
        OneDriveConnectionState.LOADING);
    flush();
    assertEquals('Loading…', signedInAsLabelElement.innerText);
    assertEquals('Connect', connectDisconnectButton.textContent!.trim());
    assertTrue(connectDisconnectButton.hasAttribute('disabled'));
    assertFalse(openOneDriveFolderButton.checkVisibility());
  });

  test('Update page to signed in state on OneDrive mount', async () => {
    await setupOneDrivePage({email: null});
    const signedInAsLabelElement =
        oneDrivePage.shadowRoot!.querySelector<HTMLDivElement>(
            '#signedInAsLabel')!;
    const connectDisconnectButton =
        oneDrivePage.shadowRoot!.querySelector<CrButtonElement>(
            '#oneDriveConnectDisconnect')!;
    assertEquals(
        'Add your Microsoft account', signedInAsLabelElement.innerText);
    assertEquals('Connect', connectDisconnectButton.textContent!.trim());

    // Simulate OneDrive mount: mount signal to observer and ability to return
    // an email address.
    const email = 'email@gmail.com';
    testOneDriveProxy.handler.setResultFor('getUserEmailAddress', {email});
    testOneDriveProxy.observerRemote.onODFSMountOrUnmount();

    await assertAsync(
        () => signedInAsLabelElement.innerText === 'Signed in as ' + email);
    assertEquals('Remove access', connectDisconnectButton.textContent!.trim());
  });

  test('Update page to signed out state on OneDrive unmount', async () => {
    const email = 'email@gmail.com';
    await setupOneDrivePage({email});
    const signedInAsLabelElement =
        oneDrivePage.shadowRoot!.querySelector<HTMLDivElement>(
            '#signedInAsLabel')!;
    const connectDisconnectButton =
        oneDrivePage.shadowRoot!.querySelector<CrButtonElement>(
            '#oneDriveConnectDisconnect')!;
    assertEquals('Signed in as ' + email, signedInAsLabelElement.innerText);
    assertEquals('Remove access', connectDisconnectButton.textContent!.trim());

    // Simulate OneDrive unmount: unmount signal and returns an empty email
    // address.
    testOneDriveProxy.handler.setResultFor(
        'getUserEmailAddress', {email: null});
    testOneDriveProxy.observerRemote.onODFSMountOrUnmount();

    await assertAsync(
        () =>
            signedInAsLabelElement.innerText === 'Add your Microsoft account');
    assertEquals('Connect', connectDisconnectButton.textContent!.trim());
  });

  test('Connect button click', async () => {
    await setupOneDrivePage({email: null});
    const connectDisconnectButton =
        oneDrivePage.shadowRoot!.querySelector<CrButtonElement>(
            '#oneDriveConnectDisconnect')!;
    assertEquals('Connect', connectDisconnectButton.textContent!.trim());

    connectDisconnectButton.click();
    assertEquals(
        1, testOneDriveProxy.handler.getCallCount('connectToOneDrive'));
    assertEquals(
        0, testOneDriveProxy.handler.getCallCount('disconnectFromOneDrive'));
  });

  test('Disconnect button click', async () => {
    const email = 'email@gmail.com';
    await setupOneDrivePage({email});
    const connectDisconnectButton =
        oneDrivePage.shadowRoot!.querySelector<CrButtonElement>(
            '#oneDriveConnectDisconnect')!;
    assertEquals('Remove access', connectDisconnectButton.textContent!.trim());

    connectDisconnectButton.click();
    assertEquals(
        0, testOneDriveProxy.handler.getCallCount('connectToOneDrive'));
    assertEquals(
        1, testOneDriveProxy.handler.getCallCount('disconnectFromOneDrive'));
  });

  test('Open OneDrive folder', async () => {
    const email = 'email@gmail.com';
    await setupOneDrivePage({email});
    const openOneDriveFolderButton =
        oneDrivePage.shadowRoot!.querySelector<CrButtonElement>(
            '#openOneDriveFolder')!;

    openOneDriveFolderButton.click();
    assertEquals(
        1, testOneDriveProxy.handler.getCallCount('openOneDriveFolder'));
  });
});
