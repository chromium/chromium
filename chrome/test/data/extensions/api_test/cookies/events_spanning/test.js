// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Expiration is capped at 400 days in the future, so we use 100 days here.
var TEST_EXPIRATION_DATE = Math.round(Date.now() / 1000) + 100 * 24 * 60 * 60;

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
  expirationDate: TEST_EXPIRATION_DATE,
  storeId: '1'
};

chrome.test.runTests([
  function testSet() {
    chrome.test.listenOnce(chrome.cookies.onChanged, function (info) {
      chrome.test.assertFalse(info.removed);
      chrome.test.assertEq('explicit', info.cause);
      chrome.test.assertEq(SET_REMOVE_COOKIE, info.cookie);
    });

    // The test uses this signal to create an off-the-record profile for us.
    // Once that's created we follow up with the cookie set.
    chrome.test.sendMessage('listening', function(response){
      chrome.cookies.set({
        url: 'http://a.com/path',
        name: 'testSetRemove',
        value: '42',
        expirationDate: TEST_EXPIRATION_DATE,
        storeId: '1'
      });
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
