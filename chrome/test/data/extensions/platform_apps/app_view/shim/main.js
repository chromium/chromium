// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var util = {};
var embedder = {};
embedder.baseGuestURL = '';
embedder.emptyGuestURL = '';
embedder.windowOpenGuestURL = '';
embedder.noReferrerGuestURL = '';
embedder.redirectGuestURL = '';
embedder.redirectGuestURLDest = '';
embedder.closeSocketURL = '';
embedder.tests = {};

embedder.setUp_ = function(config) {
  if (!config || !config.testServer) {
    return;
  }
  embedder.baseGuestURL = 'http://localhost:' + config.testServer.port;
  embedder.emptyGuestURL = embedder.baseGuestURL +
      '/extensions/platform_apps/web_view/shim/empty_guest.html';
};

window.runTest = function(testName, appToEmbed) {
  if (!embedder.test.testList[testName]) {
    window.console.log('Incorrect testName: ' + testName);
    embedder.test.fail();
    return;
  }

  // Run the test.
  embedder.test.testList[testName](appToEmbed);
};

var LOG = function(msg) {
  window.console.log(msg);
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
function testAppViewWithUndefinedDataShouldSucceed(appToEmbed) {
  var appview = new AppView();
  LOG('appToEmbed  ' + appToEmbed);
  document.body.appendChild(appview);
  // Step 1: Attempt to connect to a non-existent app.
  LOG('attempting to connect to non-existent app.');
  appview.connect('abc123', undefined, function(success) {
    // Make sure we fail.
    if (success) {
      LOG('UNEXPECTED CONNECTION.');
      embedder.test.fail();
      return;
    }
    LOG('failed to connect to non-existent app.');
    LOG('attempting to connect to known app.');
    // Step 2: Attempt to connect to an app we know exists.
    appview.connect(appToEmbed, undefined, function(success) {
      // Make sure we don't fail.
      if (!success) {
        LOG('FAILED TO CONNECT.');
        embedder.test.fail();
        return;
      }
      LOG('CONNECTED.');
      embedder.test.succeed();
    });
  });
};

function testAppViewRefusedDataShouldFail(appToEmbed) {
  var appview = new AppView();
  LOG('appToEmbed  ' + appToEmbed);
  document.body.appendChild(appview);
  LOG('Attempting to connect to app with refused params.');
  appview.connect(appToEmbed, { 'foo': 'bar' }, function(success) {
    // Make sure we fail.
    if (success) {
      LOG('UNEXPECTED CONNECTION.');
      embedder.test.fail();
      return;
    }
    LOG('FAILED TO CONNECT.');
    embedder.test.succeed();
  });
};

function testAppViewGoodDataShouldSucceed(appToEmbed) {
  var appview = new AppView();
  LOG('appToEmbed  ' + appToEmbed);
  document.body.appendChild(appview);
  LOG('Attempting to connect to app with good params.');
  // Step 2: Attempt to connect to an app with good params.
  appview.connect(appToEmbed, { 'foo': 'bleep' }, function(success) {
    // Make sure we don't fail.
    if (!success) {
      LOG('FAILED TO CONNECT.');
      embedder.test.fail();
      return;
    }
    LOG('CONNECTED.');
    embedder.test.succeed();
  });
};

function testAppViewMultipleConnects(appToEmbed) {
  var appview = new AppView();
  LOG('appToEmbed  ' + appToEmbed);
  document.body.appendChild(appview);
  var connections = 0;
  var callback = function(success) {
    if (!success) {
      LOG('FAILED TO CONNECT.');
      embedder.test.fail();
      return;
    }
    ++connections;
    LOG('CONNECTED. (' + connections + ' / 10)');
    if (connections == 10) {
      embedder.test.succeed();
      return;
    }
  }
  appview.connect(appToEmbed, { 'foo': 'bleep' }, callback);
  appview.connect(appToEmbed, { 'foo': 'bleep' }, callback);
  appview.connect(appToEmbed, { 'foo': 'bleep' }, callback);
  appview.connect(appToEmbed, { 'foo': 'bleep' }, callback);
  appview.connect(appToEmbed, { 'foo': 'bleep' }, callback);
  appview.connect(appToEmbed, { 'foo': 'bleep' }, callback);
  appview.connect(appToEmbed, { 'foo': 'bleep' }, callback);
  appview.connect(appToEmbed, { 'foo': 'bleep' }, callback);
  appview.connect(appToEmbed, { 'foo': 'bleep' }, callback);
  appview.connect(appToEmbed, { 'foo': 'bleep' }, callback);
};

function testAppViewConnectFollowingPreviousConnect(appToEmbed) {
  var appview = new AppView();
  LOG('appToEmbed  ' + appToEmbed);
  document.body.appendChild(appview);

  var connections = 0;

  function doAppViewConnect() {
    return new Promise(function(resolve, reject) {
      appview.connect(appToEmbed, {'foo': 'bleep'}, function(success) {
        if (success) {
          ++connections;
          LOG('CONNECTED. (' + connections + ' / 3)');
          resolve();
        } else {
          LOG('FAILED TO CONNECT.');
          reject();
        }
      });
    });
  };

  doAppViewConnect()
      .then(doAppViewConnect)
      .then(doAppViewConnect)
      .then(embedder.test.succeed, embedder.test.fail);
};

function testAppViewEmbedSelfShouldFail(appToEmbed) {
  var appview = new AppView();
  var currentapp_id = chrome.runtime.id;
  LOG('appToEmbed ' + currentapp_id);
  document.body.appendChild(appview);
  LOG('Attempting to embed self...(id=' + currentapp_id + ').');
  appview.connect(currentapp_id, undefined, function(success) {
    if (success) {
      LOG('UNEXPECTED CONNECTION.');
      embedder.test.fail();
      return;
    };
    LOG('EXPECTED REFUSAL.');
    embedder.test.succeed();
  });
};

function testCloseWithPendingEmbedRequest(appToEmbed) {
  let appview = new AppView();
  document.body.appendChild(appview);
  appview.connect(appToEmbed, { 'deferRequest': true });
  // The test continues on the C++ side.
  embedder.test.succeed();
};

function testFocusWebViewInAppView(appToEmbed) {
  let appview = new AppView();
  appview.style.border = 'solid';
  document.body.appendChild(appview);
  appview.connect(
      appToEmbed, {'runWebViewInAppViewFocusTest': true}, (success) => {
        embedder.test.assertTrue(success);
        appview.focus();
        // The test continues on the C++ side.
        embedder.test.succeed();
      });
};

function testBasicConnect(appToEmbed) {
  let appview = new AppView();
  document.body.appendChild(appview);
  appview.connect(appToEmbed, {}, (success) => {
    embedder.test.assertTrue(success);
    embedder.test.succeed();
  });
}

embedder.test.testList = {
  'testAppViewWithUndefinedDataShouldSucceed':
      testAppViewWithUndefinedDataShouldSucceed,
  'testAppViewRefusedDataShouldFail': testAppViewRefusedDataShouldFail,
  'testAppViewGoodDataShouldSucceed': testAppViewGoodDataShouldSucceed,
  'testAppViewMultipleConnects': testAppViewMultipleConnects,
  'testAppViewConnectFollowingPreviousConnect':
      testAppViewConnectFollowingPreviousConnect,
  'testAppViewEmbedSelfShouldFail': testAppViewEmbedSelfShouldFail,
  'testCloseWithPendingEmbedRequest': testCloseWithPendingEmbedRequest,
  'testFocusWebViewInAppView': testFocusWebViewInAppView,
  'testBasicConnect': testBasicConnect,
};

onload = function() {
  chrome.test.getConfig(function(config) {
    embedder.setUp_(config);
    chrome.test.sendMessage('Launched');
  });
};
