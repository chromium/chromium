// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.contextMenus.create({
  id: 'radio1',
  type: 'radio',
  title: 'Radio 1',
  onclick: function() {
    chrome.test.sendMessage('onclick radio1');
  }
}, function() {
  chrome.test.sendMessage('created radio1 item', function() {
    createSecondRadioButton();
  });
});

function createSecondRadioButton() {
  chrome.contextMenus.create({
    id: 'radio2',
    type: 'radio',
    title: 'Radio 2',
    onclick: function() {
      chrome.test.sendMessage('onclick radio2');
    }
  }, function() {
    chrome.test.sendMessage('created radio2 item', function() {
      createNormalMenuItem();
    });
  });
}

function createNormalMenuItem() {
  chrome.contextMenus.create({
    id: 'item1',
    title: 'Item 1',
    onclick: function() {
      chrome.test.sendMessage('onclick normal item');
      chrome.contextMenus.update('radio1', {checked: false}, function() {
        chrome.test.sendMessage('radio1 updated');
      });
    }
  }, function() {
    chrome.test.sendMessage('created normal item', function() {
      createSecondNormalMenuItem();
    });
  });
}

function createSecondNormalMenuItem() {
  chrome.contextMenus.create({
    id: 'item2',
    title: 'Item 2',
    onclick: function() {
      chrome.test.sendMessage('onclick second normal item');
      chrome.contextMenus.update('radio2', {checked: false}, function() {
        chrome.test.sendMessage('radio2 updated');
      });
    }
  }, function() {
    chrome.test.sendMessage('created second normal item');
  });
}
