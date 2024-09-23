// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var TEST_MESSAGE = 'test_message';

function assertValidExternalMessageSender(sender) {
  chrome.test.assertEq(chrome.runtime.id, sender.id);
  chrome.test.assertEq(undefined, sender.tlsChannelId);
  // TODO(crbug.com/41441298): Enable after the API implementation gets fixed:
  // chrome.test.assertEq(undefined, sender.tab);
  // chrome.test.assertEq(undefined, sender.frameId);
  // chrome.test.assertEq(undefined, sender.url);
}

chrome.test.runTests([
  // Test that the API that allows to directly communicate with other content
  // scripts is unavailable.
  function testNoTabsMessagingApi() {
    chrome.test.assertTrue(!chrome.tabs);
    chrome.test.succeed();
  },
  // Test that we can successfully send a message to an extension that doesn't
  // have custom externally_connectable manifest property.
  function testMessageToExtensionAllowingByDefault() {
    chrome.runtime.sendMessage(
        'badpbjaedophlnacllhobhnbcgomhbcd',
        TEST_MESSAGE,
        function(response) {
          chrome.test.assertEq(TEST_MESSAGE, response.receivedMessage);
          assertValidExternalMessageSender(response.receivedSender);
          chrome.test.succeed();
        });
  },
  // Test that we can successfully send a message to an extension that specifies
  // our extension ID in its externally_connectable manifest property.
  function testMessageToAllowingExtension() {
    chrome.runtime.sendMessage(
        'pmnfaklgffejbafjijfofbcianldmhci',
        TEST_MESSAGE,
        function(response) {
          chrome.test.assertEq(TEST_MESSAGE, response.receivedMessage);
          assertValidExternalMessageSender(response.receivedSender);
          chrome.test.succeed();
        });
  },
  // Test that we cannot send a message to an extension that has custom
  // externally_connectable manifest property, but doesn't contain our extension
  // ID there.
  function testMessageToDenyingExtension() {
    chrome.runtime.sendMessage(
        'gcdagggcealpldjgchchljhjlhikcmco',
        TEST_MESSAGE,
        function(response) {
          chrome.test.assertLastError('Could not establish connection. ' +
                                      'Receiving end does not exist.');
          chrome.test.succeed();
        });
  },
]);
