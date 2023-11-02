// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = {};
embedder.test = {};

embedder.test.fail = function() {
  chrome.test.sendMessage('TEST_FAILED');
};

// window.* exported functions begin.
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

var LOG = function(msg) {
  window.console.log(msg);
};

function TestState(opt_guestFullscreenChangeType,
                   opt_didEmbedderGoFullscreen) {
  this.guestFullscreenChangeType = opt_guestFullscreenChangeType || "none";
  this.didEmbedderGoFullscreen = opt_didEmbedderGoFullscreen || false;
};

TestState.prototype.equals = function(other) {
  return this.guestFullscreenChangeType === other.guestFullscreenChangeType &&
         this.didEmbedderGoFullscreen === other.didEmbedderGoFullscreen;
};

/**
 * @constructor
 * @param {boolean} allowFullscreen Whether to allow fullscreen permission.
 * @param {string} expectedChangeType Expected type of fullscreen change,
 *     can be one of "enter" or "exit".
 * @param {boolean} expectEmbedderToGoFullscreen Whether or not we expect
 *     to see fullscreen change event on the embedder/app.
 */
function Tester(allowFullscreen,
                expectedChangeType,
                expectedDidEmbedderGoFullscreen) {
  this.registerEventHandlers_();

  this.allowFullscreen_ = allowFullscreen;

  this.currentState_ = new TestState();
  this.expectedState_ = new TestState(expectedChangeType,
                                      expectedDidEmbedderGoFullscreen);
}

Tester.prototype.registerEventHandlers_ = function() {
  document.onwebkitfullscreenchange = this.onFullscreenchange_.bind(this);
  document.onwebkitfullscreenerror = this.onFullscreenerror_.bind(this);
};

Tester.prototype.onFullscreenchange_ = function(e) {
  LOG('embedder.onFullscreenchange_');
  var didEnterFullscreen = document.webkitIsFullScreen;
  if (didEnterFullscreen) {
    this.currentState_.didEmbedderGoFullscreen = true;
  }
  this.checkIfTestPassed_();
};

Tester.prototype.onFullscreenerror_ = function(e) {
  LOG('embedder.onFullscreenerror_');
  embedder.test.fail();
};

Tester.prototype.checkIfTestPassed_ = function() {
  LOG('checkIfTestPassed_');
  if (this.currentState_.equals(this.expectedState_)) {
    chrome.test.sendMessage('FULLSCREEN_STEP_PASSED');
  }
};

Tester.prototype.runTest = function() {
  var container = document.querySelector('#webview-tag-container');
  var webview = new WebView();
  webview.style.width = '100px';
  webview.style.height = '100px';
  container.appendChild(webview);

  // Setup event handlers on webview.
  webview.addEventListener('loadstop', function() {
    LOG('webview.loadstop');
    webview.executeScript(
        {file: 'guest.js'},
        function(results) {
          if (!results || !results.length) {
            LOG('Error injecting script to webview');
            embedder.test.fail();
            return;
          }
          LOG('webview executeScript succeeded');
          chrome.test.sendMessage('TEST_PASSED');
        });
  });

  webview.addEventListener('consolemessage',
                           this.onGuestConsoleMessage_.bind(this));

  webview.addEventListener('permissionrequest', function(e) {
    if (e.permission === 'fullscreen') {
      window.console.log('permissionrequest.fullscreen');
      if (this.allowFullscreen_) {
        e.request.allow();
      } else {
        e.request.deny();
      }
    }
  }.bind(this));

  // Note that we cannot use "about:blank" since fullscreen events don't
  // seem to fire on webview document in that case.
  webview.src = 'data:text/html,<html><body></body></html>';
};

Tester.prototype.onGuestConsoleMessage_ = function(e) {
  LOG('GUEST.consolemessage: ' + e.message);
  var status = this.getFullscreenStatusFromConsoleMessage_(e.message);
  if (status && status.isFullscreenChange) {
    if (status.failed) {
      embedder.test.fail();
      return;
    }
    this.currentState_.guestFullscreenChangeType = status.changeType;
    this.checkIfTestPassed_();
  }
};

Tester.prototype.getFullscreenStatusFromConsoleMessage_ = function(m) {
  // status is of the following form:
  // 'STATUS{
  //   "isFullscreenChange": true,
  //   "changeType": "enter" or "exit",
  //   failed": true/undefined
  // }'.
  var matches = m.match(/^STATUS(.*)/);
  if (!matches) {
    return null;
  }
  return JSON.parse(matches[1]);
};

function testFullscreenAllow() {
  LOG('testFullscreenAllow');
  var tester = new Tester(true /* allow */,
                          "exit" /* expectedChangeType */,
                          false /* expectEmbedderToGoFullscreen */);
  tester.runTest();
}

function testFullscreenDeny() {
  LOG('testFullscreenDeny');
  var tester = new Tester(false /* allow */,
                          "exit" /* expectedChangeType */,
                          false /* expectEmbedderToGoFullscreen */);
  tester.runTest();
}

embedder.test.testList = {
  'testFullscreenAllow': testFullscreenAllow,
  'testFullscreenDeny': testFullscreenDeny
};

chrome.test.sendMessage('Launched');
