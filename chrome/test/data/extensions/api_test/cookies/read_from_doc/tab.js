// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function readCookie(name) {
  const nameEQ = `${name}=`;
  const cookies = document.cookie.split(';');
  for (let i = 0; i < cookies.length; i++) {
    let c = cookies[i];
    while (c.charAt(0) === ' ') {
      c = c.substring(1);
    }
    if (c.indexOf(nameEQ) === 0) {
      return c.substring(nameEQ.length);
    }
  }
  return null;
}

chrome.test.runTests([
  function readCookiesFromDoc() {
    chrome.test.assertEq('1', readCookie('a'));
    chrome.test.assertEq('2', readCookie('b'));
    chrome.test.assertEq('3', readCookie('c'));
    chrome.test.assertEq(null, readCookie('nonexistent'));
    chrome.test.succeed();
  },
]);
