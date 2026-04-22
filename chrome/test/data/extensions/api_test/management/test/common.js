// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const assertEq = chrome.test.assertEq;
const assertFalse = chrome.test.assertFalse;
const assertLastError = chrome.test.assertLastError;
const assertNoLastError = chrome.test.assertNoLastError;
const assertTrue = chrome.test.assertTrue;
const fail = chrome.test.fail;
const succeed = chrome.test.succeed;
const listenOnce = chrome.test.listenOnce;
const callback = chrome.test.callback;

function getItemNamed(list, name) {
  for (let i = 0; i < list.length; i++) {
    if (list[i].name == name) {
      return list[i];
    }
  }
  fail(`didn't find item with name: ${name}`);
  return null;
}

// Verifies that the item's name, enabled, and type properties match |name|,
// |enabled|, and |type|, and checks against any additional name/value
// properties from |additionalProperties|.
function checkItem(item, name, enabled, type, additionalProperties) {
  assertTrue(item !== null);
  assertEq(name, item.name);
  assertEq(type, item.type);
  assertEq(enabled, item.enabled);

  for (const propname in additionalProperties) {
    let value = additionalProperties[propname];
    if (typeof value === 'string') {
      value = value.replace('<ID>', item.id);
    }
    assertTrue(propname in item);
    assertEq(value, item[propname]);
  }
}

// Gets an extension/app with |name| in |list|, verifies that its enabled
// and type properties match |enabled| and |type|, and checks against any
// additional name/value properties from |additionalProperties|.
function checkItemInList(list, name, enabled, type, additionalProperties) {
  const item = getItemNamed(list, name);
  checkItem(item, name, enabled, type, additionalProperties);
}
