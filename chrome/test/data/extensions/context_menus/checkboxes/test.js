// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var menuItemId = 'item1';
var checkboxOneId = 'checkbox1';
var checkboxTwoId = 'checkbox2';

chrome.contextMenus.onClicked.addListener(function(info, tab) {
  if (info.menuItemId == menuItemId) {
    chrome.test.sendMessage('onclick normal item');
    chrome.contextMenus.update('checkbox2', {checked: false}, function() {
      chrome.test.sendMessage('checkbox2 unchecked');
    });
  }
});

function createFirstCheckbox() {
  return new Promise(function(resolve, reject) {
    chrome.contextMenus.create({
      id: checkboxOneId,
      type: 'checkbox',
      title: 'Checkbox 1',
    }, resolve);
  });
}

function createSecondCheckbox() {
  return new Promise(function(resolve, reject) {
    chrome.contextMenus.create({
      id: checkboxTwoId,
      type: 'checkbox',
      title: 'Checkbox 2',
    }, resolve);
  });
}

function checkSecondCheckbox() {
  return new Promise(function(resolve, reject) {
    chrome.contextMenus.update(checkboxTwoId, {checked: true}, resolve);
  });
}

function createNormalMenuItem() {
  return new Promise(function(resolve, reject) {
    chrome.contextMenus.create({
      id: menuItemId,
      title: 'Item 1',
    }, resolve);
  });
}

chrome.runtime.onInstalled.addListener(function(details) {
  createFirstCheckbox()
      .then(createSecondCheckbox)
      .then(checkSecondCheckbox)
      .then(createNormalMenuItem)
      .then(function() {
        chrome.test.sendMessage('Menu created');
      })});
