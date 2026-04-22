// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const pass = chrome.test.callbackPass;
const assertEq = chrome.test.assertEq;
const assertTrue = chrome.test.assertTrue;

let win;
let tab;
const inIncognitoContext = chrome.extension.inIncognitoContext;

// Lifted from the bookmarks api_test.
function compareNode(left, right) {
  // chrome.test.log('compareNode()');
  // chrome.test.log(JSON.stringify(left, null, 2));
  // chrome.test.log(JSON.stringify(right, null, 2));
  // TODO(erikkay): do some comparison of dateAdded
  if (left.id != right.id) {
    return `id mismatch: ${left.id} != ${right.id}`;
  }
  if (left.title != right.title) {
    // TODO(erikkay): This resource dependency still isn't working reliably.
    // See bug 19866.
    // return `title mismatch: ${left.title} != ${right.title}`;
    chrome.test.log(`title mismatch: ${left.title} != ${right.title}`);
  }
  if (left.url != right.url) {
    return `url mismatch: ${left.url} != ${right.url}`;
  }
  if (left.index != right.index) {
    return `index mismatch: ${left.index} != ${right.index}`;
  }
  return true;
}

// Listen to some events to make sure we don't get events from the other
// profile.

chrome.tabs.onUpdated.addListener(function(id, info, tab) {
  if (inIncognitoContext != tab.incognito) {
    chrome.test.notifyFail(
        '[FAIL] Split-mode incognito test received an event for ' +
        (tab.incognito ? 'an incognito' : 'a normal') +
        ' tab in the wrong profile.');
  }
});

chrome.runtime.onMessage.addListener(function(request, sender, sendResponse) {
  if (inIncognitoContext != sender.tab.incognito) {
    chrome.test.notifyFail(
        '[FAIL] Split-mode incognito test received a message from ' +
        (sender.tab.incognito ? 'an incognito' : 'a normal') +
        ' tab in the wrong profile.');
  }
});

chrome.test.getConfig(function(config) {
  chrome.test.runTests([
    function setupWindows() {
      // The test harness should have set us up with 2 windows: 1 incognito
      // and 1 regular. Since we are in split mode, we should only see the
      // window for our profile.
      chrome.windows.getAll({populate: true}, pass(function(windows) {
                              assertEq(1, windows.length);

                              win = windows[0];
                              tab = win.tabs[0];
                              assertEq(inIncognitoContext, win.incognito);
                            }));
    },

    // Tests that we can update an incognito tab and get the event for it.
    function tabUpdate() {
      const newUrl = 'about:blank';

      // Prepare the event listeners first.
      const done = chrome.test.listenForever(
          chrome.tabs.onUpdated, function(id, info, tab) {
            assertEq(tab.id, id);
            assertEq(inIncognitoContext, tab.incognito);
            assertEq(newUrl, tab.url);
            if (info.status == 'complete') {
              done();
            }
          });

      // Update our tab.
      chrome.tabs.update(tab.id, {url: newUrl}, pass());
    },

    // Tests content script injection to verify that the script can tell its
    // in incongnito.
    function contentScriptTestIncognito() {
      const testUrl = `http://localhost:${config.testServer.port}` +
          `/extensions/test_file.html`;

      // Test that chrome.extension.inIncognitoContext is true for incognito
      // tabs.
      chrome.tabs.create(
          {windowId: win.id, url: testUrl}, pass(function(tab) {
            chrome.tabs.executeScript(
                tab.id, {
                  code: 'chrome.runtime.sendMessage({' +
                      '  inIncognitoContext: chrome.extension.inIncognitoContext' +
                      '});'
                },
                pass(function() {
                  assertEq(undefined, chrome.runtime.lastError);
                }));
          }));

      const done = chrome.test.listenForever(
          chrome.runtime.onMessage, function(request, sender, sendResponse) {
            assertEq(inIncognitoContext, request.inIncognitoContext);
            sendResponse();
            done();
          });
    },

    // Tests that we can receive bookmarks events in both extension processes.
    async function bookmarkCreate() {
      const message = inIncognitoContext ? 'waiting_incognito' : 'waiting';
      // TODO(crbug.com/414844449): Port to desktop Android, which has different
      // concepts of parent IDs and default visibility.
      const isAndroid =
          (await chrome.runtime.getPlatformInfo()).os === 'android';
      if (isAndroid) {
        // Satisfy the C++ event listeners so other tests can proceed.
        chrome.test.sendMessage(message);
        chrome.test.succeed('skipped');
        return;
      }
      // Each process will create 1 bookmark, but expects to see updates from
      // the other process.
      const nodeNormal = {
        parentId: '1',
        title: 'normal',
        url: 'http://google.com/',
      };
      const nodeIncog = {
        parentId: '1',
        title: 'incog',
        url: 'http://google.com/',
      };
      let node = inIncognitoContext ? nodeIncog : nodeNormal;
      let count = 0;
      const done = chrome.test.listenForever(
          chrome.bookmarks.onCreated, function(id, created) {
            node = (created.title == nodeNormal.title) ? nodeNormal : nodeIncog;
            node.id = created.id;
            node.index = created.index;
            chrome.test.assertEq(id, node.id);
            chrome.test.assertTrue(compareNode(node, created));
            if (++count == 2) {
              chrome.test.log(
                  `Bookmarks created. Incognito=${inIncognitoContext}`);
              done();
            }
          });
      chrome.test.sendMessage(
          message, pass(function() {
            chrome.bookmarks.create(
                node, pass(function(results) {
                  node.id = results.id;  // since we couldn't know this going in
                  node.index = results.index;
                  chrome.test.assertTrue(
                      compareNode(node, results), 'created node != source');
                }));
          }));
    },

    // Tests that we can set cookies in both processes.
    function setDocumentCookie() {
      document.cookie = 'k=v';
      chrome.test.assertNe(-1, document.cookie.indexOf('k=v'));
      chrome.test.succeed();
    },
  ]);
});
