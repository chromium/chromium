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

    chrome.printerProvider.onGetCapabilityRequested.addListener(
        function(printerId, callback) {
          chrome.test.assertFalse(!!chrome.printerProviderInternal);
          chrome.test.assertEq('printer_id', printerId);
          chrome.test.assertTrue(!!callback);

          if (test == 'ASYNC_RESPONSE') {
            setTimeout(callback.bind(null, {'capability': 'value'}), 0);
            chrome.test.succeed();
            return;
          }

          if (test == 'IGNORE_CALLBACK') {
            chrome.test.succeed();
            return;
          }

          if (test == 'INVALID_VALUE') {
            chrome.test.assertThrows(callback, ['XXX'],
                                     'No matching signature.');
          } else if (test == 'EMPTY') {
            callback({});
          } else {
            chrome.test.assertEq('OK', test);
            callback({'capability': 'value'});
          }

          chrome.test.assertThrows(
              callback,
              [{'cap': 'value'}],
              'Event callback must not be called more than once.');

          chrome.test.succeed();
        });

    chrome.test.sendMessage('ready');
  }]);
});
