// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = {};
embedder.tests = {};

window.runTest = function(testName) {
  if (!embedder.test.testList[testName]) {
    console.log('Incorrect testName: ' + testName);
    embedder.test.fail();
    return;
  }

  // Run the test.
  embedder.test.testList[testName]();
};
// window.* exported functions end.

/** @private */
embedder.setUpGuest_ = function() {
  document.querySelector('#webview-tag-container').innerHTML =
      '<webview style="width: 100px; height: 100px;"></webview>';
  var webview = document.querySelector('webview');
  if (!webview) {
    embedder.test.fail('No <webview> element created');
  }
  return webview;
};

embedder.getHTMLForGuestWithTitle_ = function(title) {
  var html =
      'data:text/html,' +
      '<html><head><title>%s</title></head>' +
      '<body>hello world</body>' +
      '</html>';
  return html.replace('%s', title);
};

embedder.test = {};
embedder.test.succeed = function() {
  chrome.test.sendMessage('TEST_PASSED');
};

embedder.test.fail = function() {
  chrome.test.sendMessage('TEST_FAILED');
};

embedder.test.assertEq = function(a, b) {
  if (a != b) {
    console.log('assertion failed: ' + a + ' != ' + b);
    embedder.test.fail();
  }
};

embedder.test.assertTrue = function(condition) {
  if (!condition) {
    console.log('assertion failed: true != ' + condition);
    embedder.test.fail();
  }
};

embedder.test.assertFalse = function(condition) {
  if (condition) {
    console.log('assertion failed: false != ' + condition);
    embedder.test.fail();
  }
};

// Tests begin.

function testNavigation() {
  var webview = embedder.setUpGuest_();

  var step = 1;
  console.log('run step: ' + step);

  // Verify that canGoBack and canGoForward work as expected.
  var runStep2 = function() {
    step = 2;
    console.log('run step: ' + step);
    webview.executeScript({
      code: 'document.title'
    }, function(results) {
      embedder.test.assertEq('step1', results[0]);
      embedder.test.assertFalse(webview.canGoBack());
      embedder.test.assertFalse(webview.canGoForward());
      webview.src = embedder.getHTMLForGuestWithTitle_('step2');
    });
  };

  // Verify that canGoBack and canGoForward work as expected.
  var runStep3 = function() {
    step = 3;
    console.log('run step: ' + step);
    webview.executeScript({
      code: 'document.title'
    }, function(results) {
      embedder.test.assertEq('step2', results[0]);
      embedder.test.assertTrue(webview.canGoBack());
      embedder.test.assertFalse(webview.canGoForward());
      webview.back();
    });
  };

  // Verify that webview.back works as expected.
  var runStep4 = function() {
    step = 4;
    console.log('run step: ' + step);
    webview.executeScript({
      code: 'document.title'
    }, function(results) {
      embedder.test.assertEq('step1', results[0]);
      embedder.test.assertFalse(webview.canGoBack());
      embedder.test.assertTrue(webview.canGoForward());
      webview.forward();
    });
  };

  // Verify that webview.forward works as expected.
  var runStep5 = function() {
    step = 5;
    console.log('run step: ' + step);
    webview.executeScript({
      code: 'document.title'
    }, function(results) {
      embedder.test.assertEq('step2', results[0]);
      embedder.test.assertTrue(webview.canGoBack());
      embedder.test.assertFalse(webview.canGoForward());
      webview.src = embedder.getHTMLForGuestWithTitle_('step3');
    });
  };

  // Navigate one more time to allow for interesting uses of webview.go.
  var runStep6 = function() {
    step = 6;
    console.log('run step: ' + step);
    webview.executeScript({
      code: 'document.title'
    }, function(results) {
      embedder.test.assertEq('step3', results[0]);
      embedder.test.assertTrue(webview.canGoBack());
      embedder.test.assertFalse(webview.canGoForward());
      webview.go(-2);
    });
  };

  // Verify that webview.go works as expected. Test the forward key.
  var runStep7 = function() {
    step = 7;
    console.log('run step: ' + step);
    webview.executeScript({
      code: 'document.title'
    }, function(results) {
      embedder.test.assertEq('step1', results[0]);
      embedder.test.assertFalse(webview.canGoBack());
      embedder.test.assertTrue(webview.canGoForward());

      // Test the callbacks of webview.go/webview.forward/webview.back.
      webview.removeEventListener('loadstop', onLoadStop);
      webview.go(3, function(success) {
        embedder.test.assertFalse(success);
        webview.back(function(success) {
          embedder.test.assertFalse(success);
          webview.forward(function(success) {
            embedder.test.assertTrue(success);
            embedder.test.succeed();
          });
        });
      });
    });
  };

  var onLoadStop = function(e) {
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
      case 4:
        runStep5();
        break;
      case 5:
        runStep6();
        break;
      case 6:
        runStep7();
        break;
      default:
        console.log('unexpected step: ' + step);
        embedder.test.fail();
    }
  };
  webview.addEventListener('loadstop', onLoadStop);
  webview.src = embedder.getHTMLForGuestWithTitle_('step1');
}

function testBackForwardKeys() {
  var webview = embedder.setUpGuest_();

  var step = 1;
  console.log('run step: ' + step);

  // Verify that canGoBack and canGoForward work as expected.
  var runStep2 = function() {
    step = 2;
    console.log('run step: ' + step);
    webview.executeScript({
      code: 'document.title'
    }, function(results) {
      embedder.test.assertEq('step1', results[0]);
      embedder.test.assertFalse(webview.canGoBack());
      embedder.test.assertFalse(webview.canGoForward());
      webview.src = embedder.getHTMLForGuestWithTitle_('step2');
    });
  };

  // Verify that webview.go works as expected. Test the forward key.
  var runStep3 = function() {
    step = 3;
    console.log('run step: ' + step);
    webview.executeScript({
      code: 'document.title'
    }, function(results) {
      embedder.test.assertEq('step2', results[0]);
      embedder.test.assertTrue(webview.canGoBack());
      embedder.test.assertFalse(webview.canGoForward());
      // Focus the webview to make sure it gets the forward key.
      webview.focus();
      chrome.test.sendMessage('ReadyForBackKey');
    });
  };

  var runStep4 = function() {
    step = 4;
    console.log('run step: ' + step);
    webview.executeScript({
      code: 'document.title'
    }, function(results) {
      embedder.test.assertEq('step1', results[0]);
      embedder.test.assertFalse(webview.canGoBack());
      embedder.test.assertTrue(webview.canGoForward());
      chrome.test.sendMessage('ReadyForForwardKey');
    });
  };

  var runStep5 = function() {
    step = 5;
    console.log('run step: ' + step);
    webview.executeScript({
      code: 'document.title'
    }, function(results) {
      embedder.test.assertEq('step2', results[0]);
      embedder.test.assertTrue(webview.canGoBack());
      embedder.test.assertFalse(webview.canGoForward());
      embedder.test.succeed();
    });
  };

  var onLoadStop = function(e) {
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
      case 4:
        runStep5();
        break;
      default:
        console.log('unexpected step: ' + step);
        embedder.test.fail();
    }
  };
  webview.addEventListener('loadstop', onLoadStop);
  webview.src = embedder.getHTMLForGuestWithTitle_('step1');
}

embedder.test.testList = {
  'testNavigation': testNavigation,
  'testBackForwardKeys': testBackForwardKeys
};

onload = function() {
  chrome.test.getConfig(function(config) {
    chrome.test.sendMessage("Launched");
  });
};
