// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy} from 'chrome://access-code-cast/browser_proxy.js';

suite('BrowserProxyTest', () => {
  let proxy;

  setup(() => {
    PolymerTest.clearBody();
    BrowserProxy.setInstance(new BrowserProxy(true));
    proxy = BrowserProxy.getInstance();
  });

  test('close sends correct message', () => {
    const chromeSend = chrome.send;

    let receivedMessage = 'none';

    // chrome.send is used for test implementation, so we retain its function
    const mockChromeSend = (message, args) => {
      receivedMessage = message;
      chromeSend(message, args);
    };

    chrome.send = mockChromeSend;
    proxy.closeDialog();
    assertEquals(receivedMessage, 'dialogClose');

    // restore chrome.send
    chrome.send = chromeSend;
  });

  test('isDialog returns correct values', () => {
    const chromeGetVariableValue = chrome.getVariableValue;

    let mockChromeGetVariableValue = (message) => {
      if (message === 'dialogArguments') {
        return '{testValue: "test"}';
      }
    };

    chrome.getVariableValue = mockChromeGetVariableValue;

    assertEquals(proxy.isDialog(), true);

    mockChromeGetVariableValue = (message) => {
      if (message === 'dialogArguments') {
        return '';
      }
    };

    chrome.getVariableValue = mockChromeGetVariableValue;

    assertEquals(proxy.isDialog(), false);

    // restore chrome.getVariableValue;
    chrome.getVariableValue = chromeGetVariableValue;
  });

  test('getDialogArgs returns an object with correct values', () => {
    const chromeGetVariableValue = chrome.getVariableValue;
    const testObject = {
      testString: 'test',
      testNumber: 123,
    };

    const testJson = JSON.stringify(testObject);
    chrome.getVariableValue = (message) => {
      if (message === 'dialogArguments') {
        return testJson;
      }
    };

    const dialogArgs = proxy.getDialogArgs();
    assertEquals(dialogArgs.testString, testObject.testString);
    assertEquals(dialogArgs.testNumber, testObject.testNumber);
    assertDeepEquals(Object.keys(dialogArgs), Object.keys(testObject));

    chrome.getVariableValue = chromeGetVariableValue;
  });

  test('isCameraAvailable returns correct values', async () => {
    const enumerateDevices = navigator.mediaDevices.enumerateDevices;

    const mockDevicesWithCamera = [
      {kind: 'audioinput'},
      {kind: 'videoinput'},
      {kind: 'audioinput'}
    ];

    const mockDevicesNoCamera = [
      {kind: 'type1'},
      {kind: 'type2'},
    ];

    navigator.mediaDevices.enumerateDevices = async () => {
      return Promise.resolve(mockDevicesWithCamera);
    };
    assertTrue(await proxy.isCameraAvailable());

    navigator.mediaDevices.enumerateDevices = async () => {
      return Promise.resolve(mockDevicesNoCamera);
    };
    assertFalse(await proxy.isCameraAvailable());

    navigator.mediaDevices.enumerateDevices = async () => {
      return Promise.resolve([]);
    };
    assertFalse(await proxy.isCameraAvailable());

    navigator.mediaDevices.enumerateDevices = enumerateDevices;
  });

  test('isQrScanningAvailable returns correct values', async () => {
    const getBoolean = loadTimeData.getBoolean;
    const proxyBarcodeDetector = proxy.isBarcodeApiAvailable;
    const proxyCamera = proxy.isCameraAvailable;

    const mockGetBooleanEnabled = (message) => {
      if (message === 'qrScannerEnabled') {
        return true;
      }
    };

    const mockGetBooleanDisabled = (message) => {
      if (message === 'qrScannerEnabled') {
        return false;
      }
    };

    const mockIsBarcodeApiAvailableTrue = () => true;
    const mockIsBarcodeApiAvailableFalse = () => false;
    const mockIsCameraAvailableTrue = () => Promise.resolve(true);
    const mockIsCameraAvailableFalse = () => Promise.resolve(false);

    // QR scanner feature is enabled
    loadTimeData.getBoolean = mockGetBooleanEnabled;
    proxy.isBarcodeApiAvailable = mockIsBarcodeApiAvailableTrue;
    proxy.isCameraAvailable = mockIsCameraAvailableTrue;
    assertTrue(await proxy.isQrScanningAvailable());

    // QR scanner feature is disabled
    loadTimeData.getBoolean = mockGetBooleanDisabled;
    assertFalse(await proxy.isQrScanningAvailable());

    proxy.isBarcodeApiAvailable = mockIsBarcodeApiAvailableFalse;
    assertFalse(await proxy.isQrScanningAvailable());

    proxy.isCameraAvailable = mockIsCameraAvailableFalse;
    assertFalse(await proxy.isQrScanningAvailable());

    loadTimeData.getBoolean = getBoolean;
    proxy.isBarcodeApiAvailable = proxyBarcodeDetector;
    proxy.isCameraAvailable = proxyCamera;
  });
});