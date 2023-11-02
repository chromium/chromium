// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var LOG = function(msg) {
  window.console.log(msg);
};

// Wrapper class for a <webview> guest.
function Guest(id, webview) {
  this.id_ = id;
  this.webview_ = webview;
};

// Runs a single verification |step| for this test.
//
// A step is asynchronous and involves sending postMessage to a <webview> and
// receiving a reply postMessage back from the <webview>.
Guest.prototype.sendStepRequestAndWait = function(
    step, successCallback, failureCallback) {
  this.expectedMessage_ = step.expect;
  this.successCallback_ = successCallback;
  this.failureCallback_ = failureCallback;
  this.expectingResponse_ = true;
  step.id = this.id_;
  this.webview_.contentWindow.postMessage(JSON.stringify(step), '*');
};

// Runs verification steps for this test listed in |stepList|.
Guest.prototype.runIDBSteps = function(
    stepList, successCallback, failureCallback) {
  if (!stepList.length) {
    LOG('error, runIDBSteps() require at least one step to run');
    failureCallback();
    return;
  }

  var currentStep = 0;
  // Proceeds to next step upon success.
  var stepSuccessCallback = function() {
    ++currentStep;
    if (currentStep == stepList.length) {
      // All steps succeeded.
      successCallback();
      return;
    }
    this.sendStepRequestAndWait(stepList[currentStep], stepSuccessCallback,
                                failureCallback);
  }.bind(this);
  this.sendStepRequestAndWait(stepList[currentStep], stepSuccessCallback,
                              failureCallback);
};

// Post message handler for this |Guest|.
Guest.prototype.onResponse = function(responseStr) {
  if (!this.expectingResponse_) {
    return;
  }
  if (this.expectedMessage_ !== responseStr) {
    LOG('Expected response from guest[' + this.id_ + ']: ' +
        this.expectedMessage_ + ' but received: ' +
        responseStr);
    this.failureCallback_();
  } else {
    this.successCallback_();
  }
};

// Tester class to run test steps.
function Tester(port) {
  this.port_ = port;
  window.onmessage = this.onMessage_.bind(this);
  this.failed_ = false;
};

Tester.prototype.getURL = function(filename) {
  return 'http://localhost:' + this.port_ +
      '/extensions/platform_apps/web_view/isolation_indexeddb/' + filename;
};

Tester.prototype.loadGuest = function(guestInfo, callback) {
  var id = guestInfo.id;
  var partition = guestInfo.partition;
  var html = guestInfo.html;
  var js = guestInfo.js;

  var webview = document.createElement('webview');
  if (partition) {
    webview.setAttribute('partition', partition);
  }
  webview.src = this.getURL(html);

  var loadFailed = false;
  webview.onloadstop = function(e) {
    LOG('webview.onloadstop: ' + id);
    if (loadFailed) {
      return;
    }

    var guest = new Guest(id, webview);
    webview.executeScript({file: js} , function(results) {
      if (!results || !results.length) {
        loadFailed = true;
        callback(null);
      }
      callback(id, new Guest(id, webview));
    }.bind(this));
  }.bind(this);

  webview.onloadabort = function(e) {
    loadFailed = true;
    callback(undefined);
  };

  webview.onconsolemessage = function(e) {
    LOG('G: ' + e.message);
  };
  document.body.appendChild(webview);
};

Tester.prototype.loadGuests = function(guestInfoList, doneCallback) {
  var failed = false;
  var responses = [];
  var numResponses = 0;
  var completedCallack = function(id, guest) {
    if (failed) {  // We've already failed.
      return;
    }
    if (!guest) {
      // guest didn't load.
      failed = true;
      doneCallback(undefined);
      return;
    }
    responses[id] = guest;
    ++numResponses;
    if (numResponses == guestInfoList.length) {
      // We are done.
      doneCallback(responses);
    }
  };
  for (var i = 0; i < guestInfoList.length; ++i) {
    this.loadGuest(guestInfoList[i], completedCallack);
  }
};

Tester.prototype.runTest = function() {
  var guestInfoList = [
    {id: 1, html: 'storage1.html', js: 'storage.js', partition: 'partition1'},
    {id: 2, html: 'storage2.html', js: 'storage.js', partition: 'partition1'},
    {id: 3, html: 'storage3.html', js: 'storage.js'}
  ];
  this.loadGuests(guestInfoList, function doneCallback(guests) {
    if (!guests) {
      LOG('One or all guests failed to load');
      this.testFail();
      return;
    }

    LOG('guests load complete');
    this.guests_ = guests;
    // Loaded all the guests.
    this.runStep1();
  }.bind(this));
};

// Initializes the storage for the first <webview>.
Tester.prototype.runStep1 = function() {
  var guest = this.guests_[1];
  guest.runIDBSteps([
      {name: 'init', expect: 'idb created'},
      {name: 'add', params: [7, 'page1'], expect: 'addItemIDB complete'},
      {name: 'read', params: [7], expect: 'readItemIDB: page1'},
  ], this.runStep2.bind(this), this.testFail.bind(this));
};

// Initializes the storage for the second <webview>, which share a storage
// partition with the first <webview>.
Tester.prototype.runStep2 = function() {
  var guest = this.guests_[2];
  guest.runIDBSteps([
      {name: 'init', expect: 'idb open'},
      {name: 'add', params: [7, 'page2'], expect: 'addItemIDB complete'},
      {name: 'read', params: [7], expect: 'readItemIDB: page2'},
  ], this.runStep3.bind(this), this.testFail.bind(this));
};

// Reads through the first <webview> to ensure we have the second value.
Tester.prototype.runStep3 = function() {
  var guest = this.guests_[1];
  guest.runIDBSteps([
      {name: 'read', params: [7], expect: 'readItemIDB: page2'},
  ], this.runStep4.bind(this), this.testFail.bind(this));
};

// Confirms that the first two <webview>s do not affect the database
// of the main browser (embedder).
Tester.prototype.runStep4 = function() {
  var request = indexedDB.open('isolation');
  request.onsuccess = function(e) {
    var version = e.target.result.version;
    // Expect version = 1.
    if (version == 1) {
      this.runStep5();  // Continue to next step.
    } else {
      this.testFail();
    }
  }.bind(this);
  request.onerror = this.testFail.bind(this);
  request.onblocked = this.testFail.bind(this);
};

// Confirms that a third <webview>'s storage does not get affect by the
// other two <webview>s.
Tester.prototype.runStep5 = function() {
  var guest = this.guests_[3];
  guest.runIDBSteps([
      {name: 'open', params: ['isolation'], expect: 'db not found'},
  ], this.testPass(this), this.testFail.bind(this));
};

// Central postMessage handler.
Tester.prototype.onMessage_ = function(e) {
  var data = JSON.parse(e.data);
  var id = data[0];
  if (typeof id !== 'number' || id < 0 || id >= this.guests_.length) {
    LOG('unexpeced guest id: ' + id);
    this.testFail();
    return;
  }

  // Re-route message to the appropriate guest.
  this.guests_[id].onResponse(data[1]);
};

Tester.prototype.testFail = function() {
  this.failed_ = true;
  chrome.test.fail();
};

Tester.prototype.testPass = function() {
  if (this.failed_) {  // We've failed already.
    return;
  }
  chrome.test.succeed();
};

chrome.test.getConfig(function(config) {
  chrome.test.runTests([
      function indexedDBIsolation() {
        var tester = new Tester(config.testServer.port);
        tester.runTest();
      }]);
});
