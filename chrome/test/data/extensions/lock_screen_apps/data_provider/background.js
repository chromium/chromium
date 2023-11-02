// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;
var callbackFail = chrome.test.callbackFail;

chrome.app.runtime.onLaunched.addListener(function(data) {
  chrome.test.runTests([
    function launchTest() {
      chrome.test.assertTrue(!!data);
      chrome.test.assertTrue(!!data.actionData);
      chrome.test.assertEq('new_note', data.actionData.actionType);
      chrome.test.assertTrue(data.actionData.isLockScreenAction);

      chrome.app.window.create('test.html', {
        lockScreenAction: 'new_note'
      }, chrome.test.callbackPass(function(createdWindow) {
        chrome.test.listenOnce(createdWindow.onClosed,
                               chrome.test.callbackPass());
      }));
    }
  ]);
});

// Event expected to be fired in regular user context when user session starts,
// or gets unlocked, in case there are data items in lock screen storage.
// The app will run set of tests verifying that the state of the app's lock
// storage matches to the state set in the app's test.html window (i.e. in tests
// run when the app is launched to handle new lock screen note action).
chrome.lockScreen.data.onDataItemsAvailable.addListener(function() {
  var itemsInfo = [];

  var sortItems = function(infoList) {
    return  itemsInfo.sort(function(lhs, rhs) {
      if (lhs.content == rhs.content)
        return 0;
      return lhs.content < rhs.content ? -1 : 1;
    });
  };

  chrome.test.runTests([
    function createNotAvailable() {
      chrome.test.assertFalse(!!chrome.lockScreen.data.create);
      chrome.test.succeed();
    },

    function getAll() {
      chrome.lockScreen.data.getAll(callbackPass(function(items) {
        chrome.test.assertEq(3, items.length);

        items.forEach(function(item) {
          var itemInfo = {id: item.id};
          chrome.lockScreen.data.getContent(item.id, callbackPass(function(
              content) {
            var decoder = new TextDecoder();
            itemInfo.content = decoder.decode(content);
            itemsInfo.push(itemInfo);
          }));
        });
      }));
    },

    function testItemInfo() {
      chrome.test.assertEq([{
        content: ''
      }, {
        content: '1 - Created by the app.'
      }, {
        content: '2 - Created and updated by the app - final.'
      }], sortItems(itemsInfo).map(function(item) {
        return {
          content: item.content
        }
      }));
      chrome.test.succeed();
    },

    function deleteItem() {
      var sortedItems = sortItems(itemsInfo);
      // Sanity check for test preconditions.
      chrome.test.assertEq(3, sortedItems.length);

      chrome.lockScreen.data.delete(itemsInfo[0].id, callbackPass(function() {
        chrome.lockScreen.data.getAll(callbackPass(function(info) {
          chrome.test.assertEq(2, info.length);
        }));
      }));
    }
  ]);
});
