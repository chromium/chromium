// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;
var fail = chrome.test.fail;
var succeed = chrome.test.succeed;

function checkIsDefined(prop) {
  if (!chrome.identity) {
    fail('chrome.identity is not defined');
    return false;
  }
  if (!chrome.identity[prop]) {
    fail('chrome.identity.' + prop + ' is not undefined');
    return false;
  }
  return true;
}

var id = 'mnkdjmfihjjihdfnnoiojdccnnfkajpd';
var host = 'https://' + id + '.chromiumapp.org';

chrome.test.runTests([

  function testGenerateRedirectURLWithPath() {
    if (!checkIsDefined('getRedirectURL'))
      return;

    var url = chrome.identity.getRedirectURL('slashless/path');
    assertEq(host + '/slashless/path', url);

    var url = chrome.identity.getRedirectURL('/slash/path');
    assertEq(host + '/slash/path', url);

    succeed();
  },

  function testGenerateRedirectURLNoPath() {
    if (!checkIsDefined('getRedirectURL'))
      return;

    var url = chrome.identity.getRedirectURL();
    assertEq(host + '/', url);

    succeed();
  },

  function testGenerateRedirectURLemptyPath() {
    if (!checkIsDefined('getRedirectURL'))
      return;

    var url = chrome.identity.getRedirectURL('');
    assertEq(host + '/', url);

    succeed();
  },

]);
