// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Check invalid call: 'asyncBlocking' is not allowed for onBeforeRequest.
var caught = false;
try {
  chrome.webRequest.onBeforeRequest.addListener(
      function(details) {}, {urls: ['http://example.com/*/*']},
      ['asyncBlocking']);
} catch (e) {
  caught = true;
}
chrome.test.assertTrue(caught);

// Check invalid call: Some url filter has to be supplied.
caught = false;
try {
  chrome.webRequest.onBeforeRequest.addListener(
      function(details) {}, {}, ['blocking']);
} catch (e) {
  caught = true;
}
chrome.test.assertTrue(caught);

// Redirect calls from simple.html -> simple2.html.
chrome.webRequest.onBeforeRequest.addListener(function(details) {
  var url = new URL(details.url);
  // Ignore favicon requests, and don't redirect simple2.html.
  if (url.pathname == '/native_bindings/simple2.html' ||
      url.pathname == '/favicon.ico')
    return {};

  chrome.test.assertEq('/native_bindings/simple.html', url.pathname);
  chrome.test.assertNe('', url.port);
  chrome.test.assertEq('example.com:' + url.port, url.host);
  var newUrl = url.origin + '/native_bindings/simple2.html';
  return {redirectUrl: newUrl};
}, {urls: ['http://example.com:*/*']}, ['blocking']);

// Note: redirection is tested on the C++ side.
chrome.test.succeed();
