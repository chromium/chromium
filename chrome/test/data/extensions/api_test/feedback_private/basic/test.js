// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function getUserEmailTest() {
    chrome.feedbackPrivate.getUserEmail(
        chrome.test.callbackPass(function(email) {
          if (email.length > 0) {
            // If we actually have an e-mail address returned, do a simple
            // sanity check on it.
            chrome.test.assertTrue(email.indexOf('@') !== -1);
          }
        }));
  },
  function getSystemInfoTest() {
    chrome.feedbackPrivate.getSystemInformation(
        chrome.test.callbackPass(function(sysinfo) {
          // Make sure we get 'some' system info.
          chrome.test.assertTrue(sysinfo.length >= 1);
        }));
  },
  function sendFeedbackTest() {
    const feedbackInfo = {
      description: 'This is a test description',
      sendHistograms: false,
    };
    chrome.feedbackPrivate.sendFeedback(
        feedbackInfo, chrome.test.callbackPass(function(status) {
          chrome.test.assertEq(chrome.feedbackPrivate.Status.SUCCESS, status);
        }));
  },
  function sendFeedbackWithoutAttachmentDataTest() {
    for (const attachedFile
             of [{name: 'attachment.txt'},
                 {name: 'attachment.txt', data: undefined},
                 {name: 'attachment.txt', data: null},
    ]) {
      chrome.test.assertThrows(
          () => chrome.feedbackPrivate.sendFeedback({
            description: 'attachment',
            attachedFile,
          }),
          /Attached file data must be a Blob\./);
    }
    chrome.test.succeed();
  },
  function sendFeedbackWithFakeBlobsTest() {
    // This fake Blob constructor is necessary to get past schema validation
    // and instead reach hook validation (rejection).
    function Blob() {}

    chrome.test.assertThrows(
        () => chrome.feedbackPrivate.sendFeedback({
          description: 'attachment',
          attachedFile: {name: 'attachment.txt', data: new Blob()},
        }),
        /Attached file data must be a Blob\./);
    chrome.test.assertThrows(
        () => chrome.feedbackPrivate.sendFeedback({
          description: 'screenshot',
          screenshot: new Blob(),
        }),
        /Screenshot must be a Blob\./);
    chrome.test.succeed();
  },
  function sendFeedbackWithAttachmentTest() {
    chrome.feedbackPrivate.sendFeedback(
        {
          description: 'attachment',
          attachedFile: {
            name: 'C:\\fakepath\\attachment.txt',
            data: new Blob(['attached file contents']),
          },
        },
        chrome.test.callbackPass(function(status) {
          chrome.test.assertEq(chrome.feedbackPrivate.Status.SUCCESS, status);
        }));
  },
  async function sendFeedbackWithScreenshotTest() {
    const result = await chrome.feedbackPrivate.sendFeedback({
      description: 'screenshot',
      screenshot: new Blob(['screenshot contents']),
    });
    chrome.test.assertEq(chrome.feedbackPrivate.Status.SUCCESS, result.status);
    chrome.test.succeed();
  },
  async function sendFeedbackWithBothTest() {
    const file = new Blob(['attached file contents']);
    const screenshot = new Blob(['screenshot contents']);
    const result = await chrome.feedbackPrivate.sendFeedback({
      description: 'both',
      attachedFile: {name: 'attachment.txt', data: file},
      screenshot,
    });
    chrome.test.assertEq(chrome.feedbackPrivate.Status.SUCCESS, result.status);
    chrome.test.assertEq('attached file contents', await file.text());
    chrome.test.assertEq('screenshot contents', await screenshot.text());
    chrome.test.succeed();
  },
]);
