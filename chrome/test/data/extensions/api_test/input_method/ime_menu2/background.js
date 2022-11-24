// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var menuItems = [{
  id: 'menu_a',
  label: 'MENU A',
  style: 'check',
  visible: true,
  checked: false,
  enabled: true
  }, {
    id: 'menu_b',
    label: 'MENU b',
    style: 'check',
    visible: true,
    checked: false,
    enabled: true
}];

var menuItemsUpdate = [{
  id: 'menu_a',
  label: 'MENU A',
  style: 'check',
  visible: true,
  checked: true,
  enabled: true
  }, {
    id: 'menu_b',
    label: 'MENU b',
    style: 'check',
    visible: false,
    checked: false,
    enabled: false
}];

var menuItemsActivated = [{
  id: 'menu_a',
  label: 'MENU A',
  style: 'check',
  visible: true,
  checked: true,
  enabled: true
  }, {
    id: 'menu_b',
    label: 'MENU b',
    style: 'check',
    visible: true,
    checked: true,
    enabled: true
}];

var compareMenuItems = function(items1, items2) {
  chrome.test.assertEq(items1.length, items2.length);
  for (var i = 0; i < items1.length; i++) {
    chrome.test.assertEq(items1[i]['id'], items2[i]['id']);
    chrome.test.assertEq(items1[i]['label'], items2[i]['label']);
    chrome.test.assertEq(items1[i]['style'], items2[i]['style']);
    chrome.test.assertEq(items1[i]['visible'], items2[i]['visible']);
    chrome.test.assertEq(items1[i]['checked'], items2[i]['checked']);
    chrome.test.assertEq(items1[i]['enabled'], items2[i]['enabled']);
  }
};

chrome.test.runTests([
  function testActivateAndListChange() {
    var list_change_count = 0;
    chrome.input.ime.onActivate.addListener(function(engineID) {
      chrome.test.sendMessage('activated');
    });
    chrome.inputMethodPrivate.onImeMenuListChanged.addListener(function() {
      ++list_change_count;
      if (list_change_count == 2) {
        chrome.test.sendMessage('list_change');
        chrome.test.succeed();
      }
    });
  },
  function testSetAndUpdateMenuItems() {
    var onMenuItemChangeCount = 0;
    chrome.inputMethodPrivate.onImeMenuItemsChanged.addListener(
      function(engineID, items) {
        chrome.test.assertEq('test', engineID);
        if (onMenuItemChangeCount == 0) {
          compareMenuItems(menuItems, items);
          ++onMenuItemChangeCount;
        }
        else {
          compareMenuItems(menuItemsUpdate, items);
          chrome.test.sendMessage('get_menu_update');
          chrome.test.succeed();
        }
      }
    );
    chrome.input.ime.setMenuItems({
      engineID: 'test',
      items: menuItems
    });
    chrome.input.ime.updateMenuItems({
      engineID: 'test',
      items: menuItemsUpdate
    });
  }
]);
