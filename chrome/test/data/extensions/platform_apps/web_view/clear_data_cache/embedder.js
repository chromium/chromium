// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var LOG = function(var_args) {
  window.console.log(Array.prototype.slice.call(arguments));
};

function ClearDataTester() {
  this.webview_ = null;
  this.id_ = '';

  this.inlineClickCalled_ = false;
  this.globalClickCalled_ = false;

  // Used for createThreeMenuItems().
  this.numItemsCreated_ = 0;

  this.failed_ = false;
}

ClearDataTester.prototype.setWebview = function(webview) {
  this.webview_ = webview;
  this.webview_.onconsolemessage = this.onGuestConsoleMessage_.bind(this);
};

ClearDataTester.prototype.onGuestConsoleMessage_ = function(e) {
  LOG('G:', e.message);
  if (e.message == 'ERROR') {
    this.fail();
  }
};

ClearDataTester.prototype.requestXhrFromWebView_ = function() {
  var msg = ['sendXhr'];
  this.webview_.contentWindow.postMessage(JSON.stringify(msg), '*');
};

ClearDataTester.prototype.fail = function() {
  chrome.test.sendMessage('TEST_FAILED');
};

ClearDataTester.prototype.pass = function() {
  chrome.test.sendMessage('TEST_PASSED');
};

// This test instructs a <webview> to load same resource request via xhr
// multiple times. That makes some of those requests to be served from
// http cache.
// Calling clearData{cache: true} resets the cache and next request
// from the same resource should not be served from cache.
ClearDataTester.prototype.testClearDataCache = function() {
  // Request same resource multiple times from <webview>, latter
  // ones would be served from cache.
  var responseCount = 0;
  var servedFromCacheCount = 0;

  var responseStartedHandler = function(details) {
    LOG('onResponseStarted, url:', details.url,
        'fromCache:', details.fromCache);
    if (details.url.indexOf('/cache-control-response') == -1) {
      return;
    }

    ++responseCount;
    if (details.fromCache) {
      ++servedFromCacheCount;
    }

    if (responseCount == 5) {
      // We should see some request getting served from cache.
      if (servedFromCacheCount <= 0) {
        this.fail();
        return;
      }

      // Clear cache from <webview>.
      this.webview_.clearData(
          {since: 10}, {'cache': true}, function doneCallback() {
            LOG('clearData done');
            this.requestXhrFromWebView_();
            // Now request the same resource again, this time it should
            // not be served from cache.
            this.requestXhrFromWebView_();
          }.bind(this));
    } else if (responseCount == 6) {
      if (details.fromCache) {
        // Response received after clearData should not be served from cache.
        this.fail();
      } else {
        this.pass();
      }
    }
  }.bind(this);

  this.webview_.request.onResponseStarted.addListener(
      responseStartedHandler, {urls: ['<all_urls>']});

  this.webview_.addEventListener('loadstop', () => {
    for (var i = 0; i < 5; ++i) {
      this.requestXhrFromWebView_();
    }
  });

  // Load the actual guest URL to start the test.
  var guestURL = this.webview_.getAttribute('src').replace(
                    '/empty_guest.html', '/guest.html');
  this.webview_.setAttribute('src', guestURL);
};

var tester = new ClearDataTester();

// window.* exported functions begin.
window.runTest = function(testName) {
  switch (testName) {
    case 'testClearCache':
      tester.testClearDataCache();
      break;
    default:
      LOG('curious test to run:', testName);
      tester.fail();
      break;
  }
};
// window.* exported functions end.

function setUpTest(emptyGuestURL, doneCallback) {
  var webview = document.createElement('webview');

  var listener = function(e) {
    webview.removeEventListener('loadstop', listener);
    LOG('webview has loaded.');
    doneCallback(webview);
  };
  webview.addEventListener('loadstop', listener);

  webview.setAttribute('src', emptyGuestURL);
  document.body.appendChild(webview);
}

onload = function() {
  chrome.test.getConfig(function(config) {
    LOG('config: ' + config.testServer.port);
    var emptyGuestURL = 'http://localhost:' + config.testServer.port +
        '/extensions/platform_apps/web_view/clear_data_cache/empty_guest.html';
    setUpTest(emptyGuestURL, function(webview) {
      LOG('Guest load completed.');
      //chrome.test.sendMessage('WebViewTest.LAUNCHED');
      chrome.test.sendMessage('Launched');
      tester.setWebview(webview);
    });
  });
};
