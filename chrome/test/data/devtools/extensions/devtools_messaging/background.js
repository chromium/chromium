// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function onRequest(request, sender, callback) {
  var response = JSON.stringify(request);
  callback("onRequest callback: " + response);
}

chrome.extension.onRequest.addListener(onRequest);

function onMessage(port, message) {
  port.postMessage("port.onMessage: " + message);
}

function onConnect(port) {
  port.onMessage.addListener(onMessage.bind(null, port));
}

chrome.runtime.onConnect.addListener(onConnect);
