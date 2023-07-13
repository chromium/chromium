// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy, DialogCloseReason} from 'chrome://access-code-cast/browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

declare const chrome: {
  send(message: string, args: any): void,
  getVariableValue(variable: string): string,
};

suite('BrowserProxyTest', () => {
  let proxy: BrowserProxy;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxy.setInstance(new BrowserProxy(true));
    proxy = BrowserProxy.getInstance();
  });

  test('close sends correct message', () => {
    const chromeSend = chrome.send;

    let receivedMessage = 'none';

    // chrome.send is used for test implementation, so we retain its function
    const mockChromeSend = (message: string, args: any) => {
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

    let mockChromeGetVariableValue: (message: string) => string =
    (message: string) => {
      if (message === 'dialogArguments') {
        return '{testValue: "test"}';
      }

      return 'NOT REACHED';
    };

    chrome.getVariableValue = mockChromeGetVariableValue;

    assertEquals(proxy.isDialog(), true);

    mockChromeGetVariableValue = (message: string) => {
      if (message === 'dialogArguments') {
        return '';
      }

      return 'NOT REACHED';
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

      return '';
    };

    const dialogArgs = proxy.getDialogArgs();
    assertEquals(dialogArgs.testString, testObject.testString);
    assertEquals(dialogArgs.testNumber, testObject.testNumber);
    assertDeepEquals(Object.keys(dialogArgs), Object.keys(testObject));

    chrome.getVariableValue = chromeGetVariableValue;
  });

  test('isCameraAvailable returns correct values', async () => {
    const enumerateDevices = navigator.mediaDevices.enumerateDevices;

    const mockDevicesWithCamera: MediaDeviceInfo[] = [
      {
        kind: 'audioinput',
        deviceId: 'test',
        groupId: 'test',
        label: 'test',
        toJSON: () => {},
      },
      {
        kind: 'videoinput',
        deviceId: 'test',
        groupId: 'test',
        label: 'test',
        toJSON: () => {},
      },
      {
        kind: 'audioinput',
        deviceId: 'test',
        groupId: 'test',
        label: 'test',
        toJSON: () => {},
      },
    ];

    const mockDevicesNoCamera: MediaDeviceInfo[] = [
      {
        kind: 'audioinput',
        deviceId: 'test',
        groupId: 'test',
        label: 'test',
        toJSON: () => {},
      },
      {
        kind: 'audioinput',
        deviceId: 'test',
        groupId: 'test',
        label: 'test',
        toJSON: () => {},
      },
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
    const proxyBarcodeDetector = proxy.isBarcodeApiAvailable;
    const proxyCamera = proxy.isCameraAvailable;

    const mockIsBarcodeApiAvailableTrue = () => true;
    const mockIsBarcodeApiAvailableFalse = () => false;
    const mockIsCameraAvailableTrue = () => Promise.resolve(true);
    const mockIsCameraAvailableFalse = () => Promise.resolve(false);

    // QR scanner feature is enabled
    loadTimeData.overrideValues({'qrScannerEnabled': true});
    proxy.isBarcodeApiAvailable = mockIsBarcodeApiAvailableTrue;
    proxy.isCameraAvailable = mockIsCameraAvailableTrue;
    assertTrue(await proxy.isQrScanningAvailable());

    // QR scanner feature is disabled
    loadTimeData.overrideValues({'qrScannerEnabled': false});
    assertFalse(await proxy.isQrScanningAvailable());

    proxy.isBarcodeApiAvailable = mockIsBarcodeApiAvailableFalse;
    assertFalse(await proxy.isQrScanningAvailable());

    proxy.isCameraAvailable = mockIsCameraAvailableFalse;
    assertFalse(await proxy.isQrScanningAvailable());

    proxy.isBarcodeApiAvailable = proxyBarcodeDetector;
    proxy.isCameraAvailable = proxyCamera;
  });

  test('metrics are properly recorded', () => {
    const realChromeSend = chrome.send;
    let receivedMessage: string;
    let receivedParams: any[]|undefined;
    const mockChromeSend = (message: string, params?: any[]) => {
      receivedMessage = message;
      receivedParams = params;
    };
    chrome.send = mockChromeSend;

    BrowserProxy.recordAccessCodeEntryTime(1000);
    assertEquals(receivedMessage!, 'metricsHandler:recordMediumTime');
    assertEquals(receivedParams![0], 'AccessCodeCast.Ui.AccessCodeInputTime');
    assertEquals(receivedParams![1], 1000);

    BrowserProxy.recordCastAttemptLength(500);
    assertEquals(receivedMessage!, 'metricsHandler:recordMediumTime');
    assertEquals(receivedParams![0], 'AccessCodeCast.Ui.CastAttemptLength');
    assertEquals(receivedParams![1], 500);

    BrowserProxy.recordDialogCloseReason(DialogCloseReason.CANCEL_BUTTON);
    assertEquals(receivedMessage!, 'metricsHandler:recordInHistogram');
    assertEquals(receivedParams![0], 'AccessCodeCast.Ui.DialogCloseReason');
    assertEquals(receivedParams![1], 1);

    chrome.send = realChromeSend;
  });
});
