// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const MENU_ITEMS = [
  {
    id: 'menu_a',
    label: 'MENU A',
    style: 'check',
    visible: true,
    checked: false,
    enabled: true,
  },
  {
    id: 'menu_b',
    label: 'MENU b',
    style: 'check',
    visible: true,
    checked: false,
    enabled: true,
  }
];

const MENU_ITEMS_UPDATE = [
  {
    id: 'menu_a',
    label: 'MENU A',
    style: 'check',
    visible: true,
    checked: true,
    enabled: true,
  },
  {
    id: 'menu_b',
    label: 'MENU b',
    style: 'check',
    visible: false,
    checked: false,
    enabled: false,
  }
];

const MENU_ITEMS_ACTIVATED = [
  {
    id: 'menu_a',
    label: 'MENU A',
    style: 'check',
    visible: true,
    checked: true,
    enabled: true,
  },
  {
    id: 'menu_b',
    label: 'MENU b',
    style: 'check',
    visible: true,
    checked: true,
    enabled: true,
  }
];

const compareMenuItems = function(items1, items2) {
  chrome.test.assertEq(items1.length, items2.length);
  for (let i = 0; i < items1.length; i++) {
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
    let listChangeCount = 0;
    chrome.input.ime.onActivate.addListener(function(engineID) {
      chrome.test.sendMessage('activated');
    });
    chrome.inputMethodPrivate.onImeMenuListChanged.addListener(function() {
      ++listChangeCount;
      if (listChangeCount == 2) {
        chrome.test.sendMessage('list_change');
        chrome.test.succeed();
      }
    });
  },
  function testSetAndUpdateMenuItems() {
    let onMenuItemChangeCount = 0;
    chrome.inputMethodPrivate.onImeMenuItemsChanged.addListener(
        function(engineID, items) {
          chrome.test.assertEq('test', engineID);
          if (onMenuItemChangeCount == 0) {
            compareMenuItems(MENU_ITEMS, items);
            ++onMenuItemChangeCount;
          } else {
            compareMenuItems(MENU_ITEMS_UPDATE, items);
            chrome.test.sendMessage('get_menu_update');
            chrome.test.succeed();
          }
        },
    );
    chrome.input.ime.setMenuItems({
      engineID: 'test',
      items: MENU_ITEMS,
    });
    chrome.input.ime.updateMenuItems({
      engineID: 'test',
      items: MENU_ITEMS_UPDATE,
    });
  },
]);
