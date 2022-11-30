// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.sendMessage('loaded', function(test) {
  chrome.test.runTests([function printTest() {
    if (test == 'NO_LISTENER') {
      chrome.test.sendMessage('ready');
      chrome.test.succeed();
      return;
    }

    chrome.printerProvider.onGetUsbPrinterInfoRequested.addListener(
        function(device, callback) {
          chrome.test.assertFalse(!!chrome.printerProviderInternal);
          chrome.test.assertTrue(!!callback);

          if (test == 'EMPTY_RESPONSE') {
            callback();
          } else {
            callback({
              'id': 'usbDevice-' + device.device,
              'name': 'Test Printer',
              'description': 'This printer is a USB device.',
            });
          }

          chrome.test.assertThrows(
              callback,
              [],
              'Event callback must not be called more than once.');

          chrome.test.succeed();
        });

    chrome.test.sendMessage('ready');
  }]);
});
