// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

var backgroundJS =
  'chrome.runtime.onMessage.addListener(function(msg, _, sendResponse) {' +
  '  if (msg.sourceCheck) {' +
  '    sendResponse({label: "onMessage/SW BG."});' +
  '  }' +
  '});';

self.onfetch = function(e) {
  let requestUrl = new URL(e.request.url);
  if (requestUrl.pathname == '/background.js') {
    e.respondWith(new Response(backgroundJS));
  }
};

