// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.extension.onRequest.addListener(
  function(url, sender, sendResponse) {
    var isErrorTriggered = false;
    var req = new XMLHttpRequest();
    console.log('Requesting url: ' + url);
    req.open('GET', url, true);

    req.onload = function() {
      sendResponse({
        'event': 'load',
        'status': req.status,
        'text': req.responseText
      });
    };
    req.onerror = function() {
      isErrorTriggered = true;
      sendResponse({
        'event': 'error',
        'status': req.status,
        'text': req.responseText
      });
    };

    try {
      req.send(null);
    } catch (e) {
      if (/^https?:/i.test(url)) {
        sendResponse({
          'thrownError': 'req.send() has thrown an error for ' + url + ': ' + e
        });
      } else if (!isErrorTriggered) {
        // A NetworkError will synchronously be be thrown whenever a
        // FTP request fails. This should be handled by req.onerror.
        sendResponse({
          'thrownError': 'req.send() has thrown an error without dispatching ' +
                         'the req.onerror event for ' + url + ': ' + e
        });
      }
    }
  });

chrome.extension.sendRequest('injected');
