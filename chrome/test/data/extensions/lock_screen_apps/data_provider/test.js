// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;
var callbackFail = chrome.test.callbackFail;

var createdIds = [];
var deletedIds = [];

chrome.test.runTests([
  function createNote() {
    chrome.lockScreen.data.create(callbackPass(function(item) {
      chrome.test.assertEq(-1, createdIds.indexOf(item.id));
      createdIds.push(item.id);

      var encoder = new TextEncoder();
      var text = '1 - Created by the app.';
      chrome.lockScreen.data.setContent(
          item.id, encoder.encode(text).buffer, callbackPass());
    }));
  },

  function createAndResetNoteContent() {
    chrome.lockScreen.data.create(callbackPass(function(item) {
      chrome.test.assertEq(-1, createdIds.indexOf(item.id));
      createdIds.push(item.id);

      var encoder = new TextEncoder();
      var text = '2 - Created and updated by the app - initial.';
      chrome.lockScreen.data.setContent(
          item.id, encoder.encode(text).buffer, callbackPass(function() {
        var text = '2 - Created and updated by the app - final.';
        chrome.lockScreen.data.setContent(
            item.id, encoder.encode(text).buffer, callbackPass());
      }));
    }));
  },

  function createAndDeleteNote() {
    chrome.lockScreen.data.create(callbackPass(function(item) {
      chrome.test.assertEq(-1, createdIds.indexOf(item.id));
      createdIds.push(item.id);
      deletedIds.push(item.id);

      var encoder = new TextEncoder();
      var text = '3 - Item deleted by the app';
      chrome.lockScreen.data.setContent(
          item.id, encoder.encode(text).buffer, callbackPass(function() {
        chrome.lockScreen.data.delete(item.id, callbackPass(function() {
          chrome.lockScreen.data.setContent(
              item.id, encoder.encode('text').buffer,
              callbackFail('Not found'));
        }));
      }));
    }));
  },

  function createEmptyNote() {
    chrome.lockScreen.data.create(callbackPass(function(item) {
      chrome.test.assertEq(-1, createdIds.indexOf(item.id));
      createdIds.push(item.id);
    }));
  },

  function getAll() {
    var sortItems = function(infoList) {
      return  infoList.sort(function(lhs, rhs) {
        return lhs.id < rhs.id ? -1 : 1;
      });
    };

    chrome.lockScreen.data.getAll(callbackPass(function(items) {
      chrome.test.assertEq(
          sortItems(createdIds.filter(function(id) {
            return deletedIds.indexOf(id) < 0;
          }).map(function(id) {
            return {id: id};
          })),
          sortItems(items));
    }));
  },

  function reportReadyToClose() {
    // Notify the test runner the app window is ready to be closed - if the test
    // runner replies with 'close', close the current app window. Otherwise, the
    // test runner will close the window itself.
    // NOTE: Reporting the test success should not wait for this - the test
    //     runner should be notified of test run success before responding to
    //     this message to avoid test done message being disregarded due to app
    //     window clusure.
    chrome.test.sendMessage('readyToClose', function(response) {
      if (response === 'close')
        chrome.app.window.current().close();
    });

    chrome.test.succeed();
  },
]);
