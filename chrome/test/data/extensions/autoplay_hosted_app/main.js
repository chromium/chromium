// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function runTest() {
  var guestUrl = '/extensions/autoplay_iframe/frame.html';

  var iframe = document.querySelector('iframe');
  return new Promise(resolve => {
    iframe.addEventListener('load', function() {
      window.addEventListener('message', (e) => {
        resolve(
            ('autoplayed' == e.data || 'NotSupportedError' == e.data));
      }, {once: true});

      iframe.contentWindow.postMessage('start', '*');
    }, {once: true});

    iframe.src = guestUrl;
  });
}
