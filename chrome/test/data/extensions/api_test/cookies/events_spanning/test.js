// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test verifies that the cookies have the correct store Id in spanning
// split mode.
var SET_REMOVE_COOKIE = {
  name: 'testSetRemove',
  value: '42',
  domain: 'a.com',
  hostOnly: true,
  path: '/',
  secure: false,
  httpOnly: false,
  sameSite: chrome.cookies.SameSiteStatus.UNSPECIFIED,
  session: false,
  expirationDate: 12345678900,
  storeId: '1'
};

chrome.test.runTests([
  function testSet() {
    chrome.test.listenOnce(chrome.cookies.onChanged, function (info) {
      chrome.test.assertFalse(info.removed);
      chrome.test.assertEq('explicit', info.cause);
      chrome.test.assertEq(SET_REMOVE_COOKIE, info.cookie);
    });
    chrome.cookies.set({
      url: 'http://a.com/path',
      name: 'testSetRemove',
      value: '42',
      expirationDate: 12345678900,
      storeId: '1'
    });
  },
  function testRemove() {
    chrome.test.listenOnce(chrome.cookies.onChanged, function (info) {
      chrome.test.assertTrue(info.removed);
      chrome.test.assertEq('explicit', info.cause);
      chrome.test.assertEq(SET_REMOVE_COOKIE, info.cookie);
    });
    chrome.cookies.remove({
      url: 'http://a.com/path',
      name: 'testSetRemove',
      storeId: '1'
    });
  },
]);
