// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  var port = config ? config.testServer.port : '0';

  var link = document.getElementById('link');
  link.href =
      'http://localhost:' + port +
      '/extensions/platform_apps/url_handlers/common/target.html';

  // This click should be intercepted and redirected to the handler app
  // (pre-installed by the CPP side of the test before launching this).
  var clickEvent = document.createEvent('MouseEvents');
  clickEvent.initMouseEvent('click', true, true, window,
                            0, 0, 0, 0, 0, false, false,
                            false, false, 0, null);
  link.dispatchEvent(clickEvent);

  chrome.test.sendMessage("Launcher done");
});
