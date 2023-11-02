// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  var port = config ? config.testServer.port : '0';
  var href =
      'http://localhost:' + port +
      '/extensions/platform_apps/url_handlers/common/target.html';
  // This click should be intercepted and redirected to the handler app
  // (pre-installed by the CPP side of the test before launching this).
  window.open(href);

  chrome.test.sendMessage("Launcher done");
});
