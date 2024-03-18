// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function content_self_inject_test() {
    window.addEventListener('message', function(event) {
        var msg = event.data;
        if (msg == 'original') {
          console.log('INJECTOR: No content changed.');
          chrome.test.fail('INJECTOR: No content changed!');
        } else {
          console.log('INJECTOR: Successfully self-injected content - ' + msg);
          chrome.test.succeed();
        }
      },
      false);

    chrome.test.getConfig(function(config) {
      chrome.test.log("Creating tab...");
      var test_url = ("http://a.com:PORT/extensions/api_test" +
          "/content_scripts/other_extensions/iframe_content.html#" +
          escape(chrome.runtime.getURL("test.html")))
          .replace(/PORT/, config.testServer.port);
      console.log('Opening frame: ' + test_url);
      document.getElementById('content_frame').src = test_url;
    });
  }
]);
