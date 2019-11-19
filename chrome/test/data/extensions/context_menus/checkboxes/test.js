// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

createFirstCheckbox()
    .then(createSecondCheckbox)
    .then(checkSecondCheckbox)
    .then(createNormalMenuItem)
    .then(function() {
      chrome.test.sendMessage('Menu created');
    });

function createFirstCheckbox() {
  return new Promise(function(resolve, reject) {
    chrome.contextMenus.create({
      id: 'checkbox1',
      type: 'checkbox',
      title: 'Checkbox 1',
      onclick: function() {
        chrome.test.sendMessage('onclick checkbox 1');
      }
    }, resolve);
  });
}

function createSecondCheckbox() {
  return new Promise(function(resolve, reject) {
    chrome.contextMenus.create({
      id: 'checkbox2',
      type: 'checkbox',
      title: 'Checkbox 2',
      onclick: function() {
        chrome.test.sendMessage('onclick checkbox 2');
      }
    }, resolve);
  });
}

function checkSecondCheckbox() {
  return new Promise(function(resolve, reject) {
    chrome.contextMenus.update('checkbox2', {checked: true}, resolve);
  });
}

function createNormalMenuItem() {
  return new Promise(function(resolve, reject) {
    chrome.contextMenus.create({
      id: 'item1',
      title: 'Item 1',
      onclick: function() {
        chrome.test.sendMessage('onclick normal item');
        chrome.contextMenus.update('checkbox2', {checked: false}, function() {
          chrome.test.sendMessage('checkbox2 unchecked');
        });
      }
    }, resolve);
  });
}
