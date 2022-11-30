// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var checkSrc = function(element, expectedValue) {
  // Note that element.getAttribute('src') should not be used, it can be out of
  // sync with element.src.
  chrome.test.assertEq(expectedValue, element.src);
};

onload = function() {
  chrome.test.runTests([
    function webView() {
      var expectedSrcOne = 'data:text/html,<body>One</body>';
      var expectedSrcTwo = 'data:text/html,<body>Two</body>';
      var expectedSrcThree = 'data:text/html,<body>Three</body>';

      var step = 1;
      // For setting src, we check if both webview.setAttribute('src', ?);
      // and webview.src = ?; works properly.
      var webview = document.querySelector('webview');

      var runStep2 = function() {
        step = 2;
        chrome.test.log('run step: ' + step);
        // Check if initial src is set correctly.
        checkSrc(webview, expectedSrcOne);
        webview.setAttribute('src', expectedSrcTwo);
      };

      var runStep3 = function() {
        step = 3;
        chrome.test.log('run step: ' + step);
        // Expect the src change to be reflected.
        checkSrc(webview, expectedSrcTwo);
        // Set src attribute directly on the element.
        webview.src = expectedSrcThree;
      };

      var runStep4 = function() {
        step = 4;
        chrome.test.log('run step: ' + step);
        // Expect the src change to be reflected.
        checkSrc(webview, expectedSrcThree);
        // Set empty src, this will be ignored.
        webview.setAttribute('src', '');

        setTimeout(function() {
          // Expect empty src to be ignored.
          checkSrc(webview, expectedSrcThree);
          // Set empty src again, directly changing the src attribute.
          webview.src = '';

          setTimeout(function() {
            // Expect empty src to be ignored.
            checkSrc(webview, expectedSrcThree);
            chrome.test.succeed();
          }, 0);
        }, 0);
      };

      // Wait for navigation to complete before checking src attribute.
      webview.addEventListener('loadcommit', function(e) {
        switch (step) {
          case 1:
            runStep2();
            break;
          case 2:
            runStep3();
            break;
          case 3:
            runStep4();
            break;
          default:
            // Unchecked.
            chrome.test.fail('Unexpected step: ' + step + ' with url: ' +
                             e.url);
        }
      });
    }
  ]);
};
