// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = {};
embedder.tests = {};
embedder.baseGuestURL = '';
embedder.guestURL = '';

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

function setUpDialogTest(messageCallback, dialogHandler) {
  var guestUrl = 'data:text/html,guest';
  var webview = document.createElement('webview');

  var onLoadStop = function(e) {
    console.log('webview has loaded.');
    webview.executeScript(
      {file: 'inject_dialog.js'},
      function(results) {
        console.log('Script has been injected into webview.');
        // Establish a communication channel with the guest.
        var msg = ['connect'];
        webview.contentWindow.postMessage(JSON.stringify(msg), '*');
      });
  };
  webview.addEventListener('loadstop', onLoadStop);

  window.addEventListener('message', function(e) {
    var data = JSON.parse(e.data);
    if (data[0] == 'connected') {
      console.log('A communication channel has been established with webview.');
    }

    messageCallback(webview, data);
  });

  webview.addEventListener('dialog', function(e) {
    dialogHandler(e);
  });

  webview.setAttribute('src', guestUrl);
  document.body.appendChild(webview);
}

// Tests begin.

function testAlertDialog() {
  var messageText = '1337h@x0r';

  var messageCallback = function(webview, data) {
    if (data[0] == 'connected') {
      console.log('The alert dialog test has started.');
      var msg = ['start-alert-dialog-test', messageText];
      webview.contentWindow.postMessage(JSON.stringify(msg), '*');
      return;
    }

    if (data[0] == 'alert-dialog-done') {
      console.log(
          'webview has been unblocked after requesting an alert dialog.');
      embedder.test.succeed();
      return;
    }
  };

  var dialogHandler = function(e) {
    console.log('webview has requested a dialog.');
    embedder.test.assertEq('alert', e.messageType);
    embedder.test.assertEq(messageText, e.messageText);
    e.dialog.ok();
    console.log('The app has responded to the dialog request.');
  };

  setUpDialogTest(messageCallback, dialogHandler);
}

function testConfirmDialog() {
  var messageText = 'foobar';

  var messageCallback = function(webview, data) {
    if (data[0] == 'connected') {
      console.log('The confirm dialog test has started.');
      var msg = ['start-confirm-dialog-test', messageText];
      webview.contentWindow.postMessage(JSON.stringify(msg), '*');
      return;
    }

    if (data[0] == 'confirm-dialog-result') {
      console.log('webview has reported a result for its confirm dialog.');
      embedder.test.assertEq(true, data[1]);
      embedder.test.succeed();
      return;
    }
  };

  var dialogHandler = function(e) {
    console.log('webview has requested a dialog.');
    embedder.test.assertEq('confirm', e.messageType);
    embedder.test.assertEq(messageText, e.messageText);
    e.dialog.ok();
    console.log('The app has responded to the dialog request.');
  };

  setUpDialogTest(messageCallback, dialogHandler);
}

function testConfirmDialogCancel() {
  var messageText = 'foobar';

  var messageCallback = function(webview, data) {
    if (data[0] == 'connected') {
      console.log('The confirm dialog test has started.');
      var msg = ['start-confirm-dialog-test', messageText];
      webview.contentWindow.postMessage(JSON.stringify(msg), '*');
      return;
    }

    if (data[0] == 'confirm-dialog-result') {
      console.log('webview has reported a result for its confirm dialog.');
      embedder.test.assertEq(false, data[1]);
      embedder.test.succeed();
      return;
    }
  };

  var dialogHandler = function(e) {
    console.log('webview has requested a dialog.');
    embedder.test.assertEq('confirm', e.messageType);
    embedder.test.assertEq(messageText, e.messageText);
    e.dialog.cancel();
    console.log('The app has responded to the dialog request.');
  };

  setUpDialogTest(messageCallback, dialogHandler);
}

function testConfirmDialogDefaultCancel() {
  var messageText = 'foobar';

  var messageCallback = function(webview, data) {
    if (data[0] == 'connected') {
      console.log('The confirm dialog test has started.');
      var msg = ['start-confirm-dialog-test', messageText];
      webview.contentWindow.postMessage(JSON.stringify(msg), '*');
      return;
    }

    if (data[0] == 'confirm-dialog-result') {
      console.log('webview has reported a result for its confirm dialog.');
      embedder.test.assertEq(false, data[1]);
      embedder.test.succeed();
      return;
    }
  };

  var dialogHandler = function(e) {
    console.log('webview has requested a dialog.');
    embedder.test.assertEq('confirm', e.messageType);
    embedder.test.assertEq(messageText, e.messageText);
  };

  setUpDialogTest(messageCallback, dialogHandler);
}

function testConfirmDialogDefaultGCCancel() {
  var messageText = 'foobar';

  var messageCallback = function(webview, data) {
    if (data[0] == 'connected') {
      console.log('The confirm dialog test has started.');
      var msg = ['start-confirm-dialog-test', messageText];
      webview.contentWindow.postMessage(JSON.stringify(msg), '*');
      return;
    }

    if (data[0] == 'confirm-dialog-result') {
      console.log('webview has reported a result for its confirm dialog.');
      embedder.test.assertEq(false, data[1]);
      embedder.test.succeed();
      return;
    }
  };

  var dialogHandler = function(e) {
    console.log('webview has requested a dialog.');
    embedder.test.assertEq('confirm', e.messageType);
    embedder.test.assertEq(messageText, e.messageText);
    // Prevent default to leave cleanup in the GC's hands.
    e.preventDefault();
    window.gc({type: 'major', execution: 'async'});
  };

  setUpDialogTest(messageCallback, dialogHandler);
}

function testPromptDialog() {
  var messageText = 'bleep';
  var defaultPromptText = 'bloop';
  var returnPromptText = 'blah';

  var messageCallback = function(webview, data) {
    if (data[0] == 'connected') {
      console.log('The prompt dialog test has started.');
      var msg = ['start-prompt-dialog-test', messageText, defaultPromptText];
      webview.contentWindow.postMessage(JSON.stringify(msg), '*');
      return;
    }

    if (data[0] == 'prompt-dialog-result') {
      console.log('webview has reported a result for its prompt dialog.');
      embedder.test.assertEq(returnPromptText, data[1]);
      embedder.test.succeed();
      return;
    }
  };

  var dialogHandler = function(e) {
    console.log('webview has requested a dialog.');
    embedder.test.assertEq('prompt', e.messageType);
    embedder.test.assertEq(messageText, e.messageText);
    embedder.test.assertEq(defaultPromptText, e.defaultPromptText);
    e.dialog.ok(returnPromptText);
    console.log('The app has responded to the dialog request.');
  };

  setUpDialogTest(messageCallback, dialogHandler);
}

embedder.test.testList = {
  'testAlertDialog': testAlertDialog,
  'testConfirmDialog': testConfirmDialog,
  'testConfirmDialogDefaultCancel': testConfirmDialogDefaultCancel,
  'testConfirmDialogDefaultGCCancel': testConfirmDialogDefaultGCCancel,
  'testConfirmDialogCancel': testConfirmDialogCancel,
  'testPromptDialog': testPromptDialog
};

onload = function() {
  chrome.test.getConfig(function(config) {
    chrome.test.sendMessage('Launched');
  });
};
