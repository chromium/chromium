// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.registerServiceWorker = function() {
  navigator.serviceWorker.register('sw.js').then(function(registration) {
    return navigator.serviceWorker.ready;
  }).then(function(registration) {
    chrome.test.sendMessage('SW_READY');
  }).catch(function(err) {
    chrome.test.log('SW registration failure: ' + err);
    chrome.test.sendMessage('SW_FAILED');
  });
  return 'true';
};
