// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  var iframe = document.createElement("iframe");
  iframe.src =
      "http://127.0.0.1:" +
      config.testServer.port +
      "/extensions/api_test/window_open/popup_iframe/content.html";
  document.body.appendChild(iframe);
});
