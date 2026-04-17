// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  const guestUrl = 'http://localhost:' + config.testServer.port +
      '/extensions/autoplay_iframe/frame.html';

  const iframe = document.querySelector('iframe');
  iframe.addEventListener('load', function() {
    window.addEventListener('message', (e) => {
      chrome.test.assertTrue(
          'autoplayed' == e.data || 'NotSupportedError' == e.data);
      chrome.test.notifyPass();
    }, {once: true});

    iframe.contentWindow.postMessage('start', '*');
  }, {once: true});

  iframe.src = guestUrl;
});
