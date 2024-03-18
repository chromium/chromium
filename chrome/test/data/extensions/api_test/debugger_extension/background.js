// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;

var debuggee;
var protocolVersion = "1.3";

chrome.test.runTests([

  async function attachToWebUI() {
    const {openTab} = await import('/_test_resources/test_util/tabs_util.js');
    const tab = await openTab('chrome://version');
    const debuggee = {tabId: tab.id};
    chrome.debugger.attach(debuggee, protocolVersion,
                           fail("Cannot attach to this target."));
    chrome.tabs.remove(tab.id);
  },

  function attach() {
    var extensionId = chrome.runtime.getURL('').split('/')[2];
    debuggee = {extensionId: extensionId};
    chrome.debugger.attach(debuggee, protocolVersion, pass());
  },

  function attachToMissing() {
    var missingDebuggee = {extensionId: "foo"};
    chrome.debugger.attach(missingDebuggee, protocolVersion,
        fail("No background page with given id " +
            missingDebuggee.extensionId + "."));
  },

  function attachAgain() {
    chrome.debugger.attach(debuggee, protocolVersion,
        fail("Another debugger is already attached " +
            "to the background page with id: " + debuggee.extensionId + "."));
  },

  function detach() {
    chrome.debugger.detach(debuggee, pass());
  },

  function detachAgain() {
    chrome.debugger.detach(debuggee,
        fail("Debugger is not attached to the background page with id: " +
            debuggee.extensionId + "."));
  },

  function discoverOwnBackgroundPage() {
    chrome.debugger.getTargets(function(targets) {
      var target = targets.filter(
        function(t) {
          return t.type == 'background_page' &&
                 t.extensionId == debuggee.extensionId &&
                 t.title == 'Extension Debugger';
        })[0];
      if (target) {
        chrome.debugger.attach({targetId: target.id}, protocolVersion, pass());
      } else {
        chrome.test.fail("Cannot discover own background page");
      }
    });
  },

  function discoverWorker() {
    var workerPort = new SharedWorker("worker.js").port;
    workerPort.onmessage = function() {
      chrome.debugger.getTargets(function(targets) {
        var page = targets.filter(
            function(t) { return t.type == 'worker' })[0];
        if (page) {
          chrome.debugger.attach({targetId: page.id}, protocolVersion, pass());
        } else {
          chrome.test.fail("Cannot discover a newly created worker");
        }
      });
    };
    workerPort.start();
  }
]);
