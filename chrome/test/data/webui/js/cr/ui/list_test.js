// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {assertEquals} from '../../../chai_assert.js';
// #import {List} from 'chrome://resources/js/cr/ui/list.m.js';
// #import {decorate} from 'chrome://resources/js/cr/ui.m.js';
// #import {ArrayDataModel} from 'chrome://resources/js/cr/ui/array_data_model.m.js';
// clang-format on

function testClearPinnedItem() {
  var list = document.createElement('ul');
  list.style.position = 'absolute';
  list.style.width = '800px';
  list.style.height = '800px';
  cr.ui.decorate(list, cr.ui.List);
  document.body.appendChild(list);

  var model = new cr.ui.ArrayDataModel(['Item A', 'Item B']);
  list.dataModel = model;
  list.selectionModel.setIndexSelected(0, true);
  list.selectionModel.leadIndex = 0;
  list.ensureLeadItemExists();

  list.style.height = '0px';
  model.splice(0, 1);

  list.style.height = '800px';
  list.redraw();
  assertEquals('Item B', list.querySelectorAll('li')[0].textContent);
}

function testClickOutsideListItem() {
  const list = document.createElement('ul');
  list.style.position = 'absolute';
  list.style.width = '800px';
  list.style.height = '800px';
  cr.ui.decorate(list, cr.ui.List);
  document.body.appendChild(list);

  // Add a header inside the list.
  const header = document.createElement('h1');
  header.innerText = 'Title inside the list';
  list.appendChild(header);

  const model = new cr.ui.ArrayDataModel(['Item A', 'Item B']);
  list.dataModel = model;

  list.redraw();

  const item = list.querySelector('li');
  const span = document.createElement('span');
  span.innerText = 'some text';
  item.appendChild(span);

  // Non-LI children should return null.
  assertEquals(null, list.getListItemAncestor(header));

  // It should return null for the list itself.
  assertEquals(null, list.getListItemAncestor(list));

  // Anything inside a LI should return the LI itself.
  assertEquals(item, list.getListItemAncestor(item));
  assertEquals(item, list.getListItemAncestor(span));
}

Object.assign(window, {
  testClearPinnedItem,
  testClickOutsideListItem,
});
