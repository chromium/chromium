// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var otherId = 'ljhhihhmjomkjokmknellgbidphmahkh';

chrome.runtime.onConnectExternal.addListener(function(port) {
  port.onMessage.addListener(function(msg) {
    if (msg == 'ok_to_disconnect') {
      port.disconnect();
    } else {
      port.postMessage(msg + '_reply');
    }
  });
});

chrome.runtime.onMessageExternal.addListener(function(msg, sender, callback) {
  chrome.test.assertEq({
    id: otherId,
    url: 'chrome-extension://' + otherId + '/_generated_background_page.html',
    origin: 'chrome-extension://' + otherId
  }, sender);
  if (msg == 'hello')
    callback('hello_response');
  else
    callback();
});

// Must ensure that the listeners are active before sending the "Ready"
// message (which will cause app1 to be launched).
chrome.test.sendMessage('Ready');
