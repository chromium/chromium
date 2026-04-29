// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const embedder = {};
embedder.tests = {};
embedder.baseGuestURL = '';
embedder.guestURL = '';

window.runTest = function(testName) {
  if (!embedder.test.testList[testName]) {
    console.info('Incorrect testName: ' + testName);
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
  const webview = document.querySelector('webview');
  if (!webview) {
    embedder.test.fail('No <webview> element created');
  }
  return webview;
};

embedder.getHTMLForGuestWithTitle_ = function(title) {
  const html = 'data:text/html,' +
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
  if (a !== b) {
    console.info('assertion failed: ' + a + ' !== ' + b);
    embedder.test.fail();
  }
};

embedder.test.assertTrue = function(condition) {
  if (!condition) {
    console.info('assertion failed: true !== ' + condition);
    embedder.test.fail();
  }
};

embedder.test.assertFalse = function(condition) {
  if (condition) {
    console.info('assertion failed: false !== ' + condition);
    embedder.test.fail();
  }
};

function setUpDialogTest(messageCallback, dialogHandler) {
  const guestUrl = 'data:text/html,guest';
  const webview = document.createElement('webview');

  const onLoadStop = function(e) {
    console.info('webview has loaded.');
    webview.executeScript({file: 'inject_dialog.js'}, function(results) {
      console.info('Script has been injected into webview.');
      // Establish a communication channel with the guest.
      const msg = ['connect'];
      webview.contentWindow.postMessage(JSON.stringify(msg), '*');
    });
  };
  webview.addEventListener('loadstop', onLoadStop);

  window.addEventListener('message', function(e) {
    const data = JSON.parse(e.data);
    if (data[0] === 'connected') {
      console.info(
          'A communication channel has been established with webview.');
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

function testDialogAlert() {
  const messageText = '1337h@x0r';

  const messageCallback = function(webview, data) {
    if (data[0] === 'connected') {
      console.info('The alert dialog test has started.');
      const msg = ['start-alert-dialog-test', messageText];
      webview.contentWindow.postMessage(JSON.stringify(msg), '*');
      return;
    }

    if (data[0] === 'alert-dialog-done') {
      console.info(
          'webview has been unblocked after requesting an alert dialog.');
      embedder.test.succeed();
      return;
    }
  };

  const dialogHandler = function(e) {
    console.info('webview has requested a dialog.');
    embedder.test.assertEq('alert', e.messageType);
    embedder.test.assertEq(messageText, e.messageText);
    e.dialog.ok();
    console.info('The app has responded to the dialog request.');
  };

  setUpDialogTest(messageCallback, dialogHandler);
}

function testDialogConfirm() {
  const messageText = 'foobar';

  const messageCallback = function(webview, data) {
    if (data[0] === 'connected') {
      console.info('The confirm dialog test has started.');
      const msg = ['start-confirm-dialog-test', messageText];
      webview.contentWindow.postMessage(JSON.stringify(msg), '*');
      return;
    }

    if (data[0] === 'confirm-dialog-result') {
      console.info('webview has reported a result for its confirm dialog.');
      embedder.test.assertEq(true, data[1]);
      embedder.test.succeed();
      return;
    }
  };

  const dialogHandler = function(e) {
    console.info('webview has requested a dialog.');
    embedder.test.assertEq('confirm', e.messageType);
    embedder.test.assertEq(messageText, e.messageText);
    e.dialog.ok();
    console.info('The app has responded to the dialog request.');
  };

  setUpDialogTest(messageCallback, dialogHandler);
}

function testDialogConfirmCancel() {
  const messageText = 'foobar';

  const messageCallback = function(webview, data) {
    if (data[0] === 'connected') {
      console.info('The confirm dialog test has started.');
      const msg = ['start-confirm-dialog-test', messageText];
      webview.contentWindow.postMessage(JSON.stringify(msg), '*');
      return;
    }

    if (data[0] === 'confirm-dialog-result') {
      console.info('webview has reported a result for its confirm dialog.');
      embedder.test.assertEq(false, data[1]);
      embedder.test.succeed();
      return;
    }
  };

  const dialogHandler = function(e) {
    console.info('webview has requested a dialog.');
    embedder.test.assertEq('confirm', e.messageType);
    embedder.test.assertEq(messageText, e.messageText);
    e.dialog.cancel();
    console.info('The app has responded to the dialog request.');
  };

  setUpDialogTest(messageCallback, dialogHandler);
}

function testDialogConfirmDefaultCancel() {
  const messageText = 'foobar';

  const messageCallback = function(webview, data) {
    if (data[0] === 'connected') {
      console.info('The confirm dialog test has started.');
      const msg = ['start-confirm-dialog-test', messageText];
      webview.contentWindow.postMessage(JSON.stringify(msg), '*');
      return;
    }

    if (data[0] === 'confirm-dialog-result') {
      console.info('webview has reported a result for its confirm dialog.');
      embedder.test.assertEq(false, data[1]);
      embedder.test.succeed();
      return;
    }
  };

  const dialogHandler = function(e) {
    console.info('webview has requested a dialog.');
    embedder.test.assertEq('confirm', e.messageType);
    embedder.test.assertEq(messageText, e.messageText);
  };

  setUpDialogTest(messageCallback, dialogHandler);
}

function testDialogConfirmDefaultGCCancel() {
  const messageText = 'foobar';

  const messageCallback = function(webview, data) {
    if (data[0] === 'connected') {
      console.info('The confirm dialog test has started.');
      const msg = ['start-confirm-dialog-test', messageText];
      webview.contentWindow.postMessage(JSON.stringify(msg), '*');
      return;
    }

    if (data[0] === 'confirm-dialog-result') {
      console.info('webview has reported a result for its confirm dialog.');
      embedder.test.assertEq(false, data[1]);
      embedder.test.succeed();
      return;
    }
  };

  const dialogHandler = function(e) {
    console.info('webview has requested a dialog.');
    embedder.test.assertEq('confirm', e.messageType);
    embedder.test.assertEq(messageText, e.messageText);
    // Prevent default to leave cleanup in the GC's hands.
    e.preventDefault();
    window.gc({type: 'major', execution: 'async'});
  };

  setUpDialogTest(messageCallback, dialogHandler);
}

function testDialogPrompt() {
  const messageText = 'bleep';
  const defaultPromptText = 'bloop';
  const returnPromptText = 'blah';

  const messageCallback = function(webview, data) {
    if (data[0] === 'connected') {
      console.info('The prompt dialog test has started.');
      const msg = ['start-prompt-dialog-test', messageText, defaultPromptText];
      webview.contentWindow.postMessage(JSON.stringify(msg), '*');
      return;
    }

    if (data[0] === 'prompt-dialog-result') {
      console.info('webview has reported a result for its prompt dialog.');
      embedder.test.assertEq(returnPromptText, data[1]);
      embedder.test.succeed();
      return;
    }
  };

  const dialogHandler = function(e) {
    console.info('webview has requested a dialog.');
    embedder.test.assertEq('prompt', e.messageType);
    embedder.test.assertEq(messageText, e.messageText);
    embedder.test.assertEq(defaultPromptText, e.defaultPromptText);
    e.dialog.ok(returnPromptText);
    console.info('The app has responded to the dialog request.');
  };

  setUpDialogTest(messageCallback, dialogHandler);
}

embedder.test.testList = {
  'testDialogAlert': testDialogAlert,
  'testDialogConfirm': testDialogConfirm,
  'testDialogConfirmCancel': testDialogConfirmCancel,
  'testDialogConfirmDefaultCancel': testDialogConfirmDefaultCancel,
  'testDialogConfirmDefaultGCCancel': testDialogConfirmDefaultGCCancel,
  'testDialogPrompt': testDialogPrompt,
};

onload = function() {
  chrome.test.getConfig(function(config) {
    chrome.test.sendMessage('LAUNCHED');
  });
};
