// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.sendMessage('loaded', function(test) {
  chrome.test.runTests([function printTest() {
    if (test == 'NO_LISTENER') {
      chrome.test.sendMessage('ready');
      chrome.test.succeed();
      return;
    }

    chrome.printerProvider.onGetPrintersRequested.addListener(
        function(callback) {
          chrome.test.assertFalse(!!chrome.printerProviderInternal);
          chrome.test.assertTrue(!!callback);

          if (test == 'IGNORE_CALLBACK') {
            chrome.test.succeed();
            return;
          }

          if (test == 'INVALID_VALUE') {
            chrome.test.assertThrows(
                callback,
                ['XXX'],
                'Error validating the callback argument: '+
                'Expected an object, found string.');
          } else if (test == 'EMPTY') {
            callback([]);
          } else {
            chrome.test.assertEq('OK', test);
            callback([{
              id: 'printer1',
              name: 'Printer 1',
              description: 'Test printer'
            }, {
              id: 'printerNoDesc',
              name: 'Printer 2'
            }]);
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
