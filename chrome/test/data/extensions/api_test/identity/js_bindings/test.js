// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const assertEq = chrome.test.assertEq;
const assertTrue = chrome.test.assertTrue;
const fail = chrome.test.fail;
const succeed = chrome.test.succeed;

function checkIsDefined(prop) {
  if (!chrome.identity) {
    fail('chrome.identity is not defined');
    return false;
  }
  if (!chrome.identity[prop]) {
    fail(`chrome.identity.${prop} is not undefined`);
    return false;
  }
  return true;
}

const id = 'mnkdjmfihjjihdfnnoiojdccnnfkajpd';
const host = `https://${id}.chromiumapp.org`;

chrome.test.runTests([

  function testGenerateRedirectURLWithPath() {
    if (!checkIsDefined('getRedirectURL')) {
      return;
    }

    let url = chrome.identity.getRedirectURL('slashless/path');
    assertEq(`${host}/slashless/path`, url);

    url = chrome.identity.getRedirectURL('/slash/path');
    assertEq(`${host}/slash/path`, url);

    succeed();
  },

  function testGenerateRedirectURLNoPath() {
    if (!checkIsDefined('getRedirectURL')) {
      return;
    }

    const url = chrome.identity.getRedirectURL();
    assertEq(`${host}/`, url);

    succeed();
  },

  function testGenerateRedirectURLemptyPath() {
    if (!checkIsDefined('getRedirectURL')) {
      return;
    }

    const url = chrome.identity.getRedirectURL('');
    assertEq(`${host}/`, url);

    succeed();
  },

]);
