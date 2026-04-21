// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// browser_tests --gtest_filter=ExtensionApiTest.TabConnect

// The test tab used for all tests.
const testTab = null;

// Path to the empty HTML document, fetched from the test configuration.
const emptyDocumentURL = null;

// The could-not-establish-connection error message. Used in a number of tests.
const couldNotEstablishError =
    'Could not establish connection. Receiving end does not exist.';

const assertEq = chrome.test.assertEq;
const assertFalse = chrome.test.assertFalse;
const assertTrue = chrome.test.assertTrue;
const callbackFail = chrome.test.callbackFail;
const fail = chrome.test.fail;
const listenForever = chrome.test.listenForever;
const listenOnce = chrome.test.listenOnce;
const pass = chrome.test.callbackPass;

// Removes current windows and creates one window with tabs set to
// the urls in the array |tabUrls|. At least one url must be specified.
// The |callback| should look like function(windowId, tabIds) {...}.
function setupWindow(tabUrls, callback) {
  createWindow(tabUrls, {}, function(winId, tabIds) {
    // Remove all other windows.
    let removedCount = 0;
    chrome.windows.getAll({}, function(windows) {
      for (let i in windows) {
        if (windows[i].id != winId) {
          chrome.windows.remove(windows[i].id, function() {
            removedCount++;
            if (removedCount == windows.length - 1)
              callback(winId, tabIds);
          });
        }
      }
      if (windows.length == 1)
        callback(winId, tabIds);
    });
  });
}

// Creates one window with tabs set to the urls in the array |tabUrls|.
// At least one url must be specified.
// The |callback| should look like function(windowId, tabIds) {...}.
function createWindow(tabUrls, winOptions, callback) {
  winOptions['url'] = tabUrls;
  chrome.windows.create(winOptions, function(win) {
    const newTabIds = [];
    assertTrue(win.id > 0);
    assertEq(tabUrls.length, win.tabs.length);

    for (let i = 0; i < win.tabs.length; i++)
      newTabIds.push(win.tabs[i].id);

    callback(win.id, newTabIds);
  });
}

function waitForReady(ready) {
  chrome.test.listenOnce(chrome.runtime.onMessage, function(msg, sender) {
    assertEq('ready', msg);
    ready(sender.tab);
  });
}

chrome.test.runTests([
  function setup() {
    chrome.test.getConfig(pass(function(config) {
      emptyDocumentURL = `http://localhost:${config.testServer.port}` +
                         '/extensions/api_test/tabs/connect/empty.html';
      setupWindow([emptyDocumentURL], pass());
      waitForReady(pass(function(tab) {
        testTab = tab;
      }));
    }));
  },

  function connectMultipleConnects() {
    let connectCount = 0;
    function connect10() {
      const port = chrome.tabs.connect(testTab.id);
      listenOnce(port.onMessage, function(msg) {
        assertEq(++connectCount, msg.connections);
        if (connectCount < 10)
          connect10();
      });
      port.postMessage('GET');
    }
    connect10();
  },

  function connectName() {
    const name = 'akln3901n12la';
    const port = chrome.tabs.connect(testTab.id, {name: name});
    listenOnce(port.onMessage, function(msg) {
      assertEq(name, msg.name);

      const port = chrome.tabs.connect(testTab.id);
      listenOnce(port.onMessage, function(msg) {
        assertEq('', msg.name);
      });
      port.postMessage('GET');
    });
    port.postMessage('GET');
  },

  function connectPostMessageTypes() {
    const port = chrome.tabs.connect(testTab.id);
    // Test the content script echoes the message back.
    const echoMsg = {num: 10, string: 'hi', array: [1,2,3,4,5],
                   obj:{dec: 1.0}};
    listenOnce(port.onMessage, function(msg) {
      assertEq(echoMsg.num, msg.num);
      assertEq(echoMsg.string, msg.string);
      assertEq(echoMsg.array[4], msg.array[4]);
      assertEq(echoMsg.obj.dec, msg.obj.dec);
    });
    port.postMessage(echoMsg);
  },

  function connectPostManyMessages() {
    const port = chrome.tabs.connect(testTab.id);
    let count = 0;
    const done = function(msg) {
      assertEq(count++, msg);
      if (count == 100) {
        done();
      }
    });
    for (let i = 0; i < 100; i++) {
      port.postMessage(i);
    }
  },

  function connectToRemovedTab() {
    // Expect a disconnect event when you connect to a non-existent tab, and
    // once disconnected, expect an error while trying to post messages.
    chrome.tabs.create({}, pass(function(tab) {
      chrome.tabs.remove(tab.id, pass(function() {
        const p = chrome.tabs.connect(tab.id);
        p.onDisconnect.addListener(callbackFail(couldNotEstablishError,
                                                function() {
          try {
            p.postMessage();
            fail('Error should have been thrown.');
          } catch (e) {
            // Do nothing- an exception should be thrown.
          }
        }));
      }));
    }));
  },

  function sendRequest() {
    const request = 'test';
    chrome.tabs.sendRequest(testTab.id, request, pass(function(response) {
      assertEq(request, response);
    }));
  },

  function sendRequestToImpossibleTab() {
    chrome.tabs.sendRequest(9999, 'test', callbackFail(couldNotEstablishError));
  },

  function sendRequestToRemovedTab() {
    chrome.tabs.create({}, pass(function(tab) {
      chrome.tabs.remove(tab.id, pass(function() {
        chrome.tabs.sendRequest(tab.id, 'test',
                                callbackFail(couldNotEstablishError));
      }));
    }));
  },

  function sendRequestMultipleTabs() {
    // Regression test for crbug.com/40431153. Instruct the test tab to create
    // another tab, then send a message to each. The bug was that the message
    // is sent to both, if they're in the same process.
    //
    // The tab itself must do the open so that they share a process,
    // chrome.tabs.create doesn't guarantee that.
    chrome.tabs.sendMessage(testTab.id, {open: emptyDocumentURL});
    waitForReady(pass(function(secondTab) {
      const gotDuplicates = false;
      const messages = new Set();
      const done = function(msg) {
        if (messages.has(msg))
          gotDuplicates = true;
        else
          messages.add(msg);
      });
      chrome.tabs.sendMessage(testTab.id, {send: 'msg1'}, function() {
        chrome.tabs.sendMessage(secondTab.id, {send: 'msg2'}, function() {
          // Send an empty final message to hopefully ensure that the events
          // for msg1 and msg2 have been fired.
          chrome.tabs.sendMessage(testTab.id, {}, function() {
            assertEq(2, messages.size);
            assertTrue(messages.has('msg1'));
            assertTrue(messages.has('msg2'));
            assertFalse(gotDuplicates);
            done();
          });
        });
      });
    }));
  },
]);
