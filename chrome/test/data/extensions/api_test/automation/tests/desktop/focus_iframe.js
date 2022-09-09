// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testFocusInIframes() {
    chrome.test.getConfig(function(config) {
      var url = 'http://a.com:' + config.testServer.port + '/iframe_outer.html';
      chrome.tabs.create({url: url});

      chrome.automation.getDesktop(function(rootNode) {
        // Succeed when the button inside the iframe gets focus.
        rootNode.addEventListener('focus', function(event) {
          if (event.target.name == 'Inner') {
            chrome.test.succeed();
          }
        });

        // Poll until we get the inner button, which is in the inner frame.
        const id = setInterval(() => {
          var innerButton =
              rootNode.find({attributes: {name: 'Inner'}, role: 'button'});
          if (innerButton) {
            innerButton.focus();
            clearInterval(id);
          }
        }, 100);
      });
    });
  },
];

chrome.test.runTests(allTests);
