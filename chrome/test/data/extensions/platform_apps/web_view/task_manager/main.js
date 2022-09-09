// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var startTest = function() {
  chrome.test.sendMessage('guest-loaded');
};

chrome.test.getConfig(function(config) {
  var guestURL = 'http://localhost:' + config.testServer.port +
      '/extensions/platform_apps/web_view/task_manager/guest.html';
  document.querySelector('#webview-tag-container').innerHTML =
      '<webview style="width: 10px; height: 10px; margin: 0; padding: 0;"' +
      ' src="' + guestURL + '"' +
      '></webview>';
  startTest();
});
