// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var radio1_id = 'radio1';
var radio2_id = 'radio2';
var item1_id = 'item1';
var item2_id = 'item2';

chrome.contextMenus.onClicked.addListener(function(info, tab) {
  if (info.menuItemId == radio1_id) {
    chrome.test.sendMessage('onclick radio1');
  } else if (info.menuItemId == radio2_id) {
    chrome.test.sendMessage('onclick radio2');
  } else if (info.menuItemId == item1_id) {
    chrome.test.sendMessage('onclick normal item');
    chrome.contextMenus.update(radio1_id, {checked: false}, function() {
      chrome.test.sendMessage('radio1 updated');
    });
  } else if (info.menuItemId == item2_id) {
    chrome.test.sendMessage('onclick second normal item');
    chrome.contextMenus.update(radio2_id, {checked: false}, function() {
      chrome.test.sendMessage('radio2 updated');
    });
  }
});

function createSecondRadioButton() {
  chrome.contextMenus.create(
      {id: radio2_id, type: 'radio', title: 'Radio 2'},
      function() {
        chrome.test.sendMessage('created radio2 item', function() {
          createNormalMenuItem();
        });
      });
}

function createNormalMenuItem() {
  chrome.contextMenus.create(
      {id: item1_id, title: 'Item 1'}, function() {
        chrome.test.sendMessage('created normal item', function() {
          createSecondNormalMenuItem();
        });
      });
}

function createSecondNormalMenuItem() {
  chrome.contextMenus.create(
      {id: item2_id, title: 'Item 2' }, function() {
        chrome.test.sendMessage('created second normal item');
      });
}

chrome.contextMenus.create(
    {id: radio1_id, type: 'radio', title: 'Radio 1'}, function() {
      chrome.test.sendMessage('created radio1 item', function() {
        createSecondRadioButton();
      });
    });
