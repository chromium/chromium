// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var onMessageReply = function(message) {
  var menuId = 'my_id';
  var enabled = (message == 'start enabled');
  chrome.contextMenus.create({title: 'Extension Item 1',
                              id: menuId,
                              enabled: enabled}, function() {
    chrome.test.sendMessage('create', function(message) {
      chrome.test.assertEq('go', message);
      chrome.contextMenus.update(menuId, {'enabled': !enabled}, function() {
        chrome.test.sendMessage('update');
      });
    });
  });
};

chrome.test.sendMessage('begin', onMessageReply);
