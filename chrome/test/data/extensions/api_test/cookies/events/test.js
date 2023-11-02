// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Expiration is capped at 400 days in the future, so we use 100 days here.
var TEST_EXPIRATION_DATE = Math.round(Date.now() / 1000) + 100 * 24 * 60 * 60;

// These are the cookies we expect to see along the way.
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
  storeId: "0"
};

var OVERWRITE_COOKIE_PRE = {
  name: 'testOverwrite',
  value: '42',
  domain: 'a.com',
  hostOnly: true,
  path: '/',
  secure: false,
  httpOnly: false,
  sameSite: chrome.cookies.SameSiteStatus.UNSPECIFIED,
  session: false,
  expirationDate: TEST_EXPIRATION_DATE,
  storeId: "0"
};

var OVERWRITE_COOKIE_POST = {
  name: 'testOverwrite',
  value: '43',
  domain: 'a.com',
  hostOnly: true,
  path: '/',
  secure: false,
  httpOnly: false,
  sameSite: chrome.cookies.SameSiteStatus.UNSPECIFIED,
  session: false,
  expirationDate: TEST_EXPIRATION_DATE,
  storeId: "0"
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
      expirationDate: TEST_EXPIRATION_DATE
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
      name: 'testSetRemove'
    });
  },
  function overwriteFirstSet() {
    chrome.test.listenOnce(chrome.cookies.onChanged, function (info) {
      chrome.test.assertFalse(info.removed);
      chrome.test.assertEq('explicit', info.cause);
      chrome.test.assertEq(OVERWRITE_COOKIE_PRE, info.cookie);
    });
    chrome.cookies.set({
      url: 'http://a.com/path',
      name: 'testOverwrite',
      value: '42',
      expirationDate: TEST_EXPIRATION_DATE
    });
  },
  function overwriteSecondSet() {
    var haveRemoved = false;
    var haveSet = false;
    var done = chrome.test.listenForever(chrome.cookies.onChanged,
      function(info) {
        if (info.removed) {
          chrome.test.assertEq('overwrite', info.cause);
          chrome.test.assertEq(OVERWRITE_COOKIE_PRE, info.cookie);
          chrome.test.assertFalse(haveRemoved);
          chrome.test.assertFalse(haveSet);
          haveRemoved = true;
        } else {
          chrome.test.assertEq('explicit', info.cause);
          chrome.test.assertEq(OVERWRITE_COOKIE_POST, info.cookie);
          chrome.test.assertTrue(haveRemoved);
          chrome.test.assertFalse(haveSet);
          haveSet = true;
        }
        if (haveRemoved && haveSet) {
          done();
        }
      });
    chrome.cookies.set({
      url: 'http://a.com/path',
      name: 'testOverwrite',
      value: '43',
      expirationDate: TEST_EXPIRATION_DATE
    });
  },
  function overwriteExpired() {
    chrome.test.listenOnce(chrome.cookies.onChanged, function (info) {
      chrome.test.assertTrue(info.removed);
      chrome.test.assertEq('expired_overwrite', info.cause);
      chrome.test.assertEq(OVERWRITE_COOKIE_POST, info.cookie);
    });
    chrome.cookies.set({
      url: 'http://a.com/path',
      name: 'testOverwrite',
      value: '43',
      expirationDate: 1
    });
  }
]);
