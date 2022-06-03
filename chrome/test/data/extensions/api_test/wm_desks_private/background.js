// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Basic browser tests for the wmDesksPrivate API.
chrome.test.runTests([
  // Tests the entire work flow for template: capturing the active desk, saving
  // it as the desk template, listing it and deleting the template.
  // For now it only contains the part of capturing active desk. The others will
  // be added in following CLs.
  function testTemplateFlow() {
    chrome.wmDesksPrivate.captureActiveDeskAndSaveTemplate(
        chrome.test.callbackPass(function(deskTemplate) {
          chrome.test.assertEq(typeof deskTemplate, 'object');
          chrome.test.assertTrue(deskTemplate.hasOwnProperty('templateUuid'));
          chrome.test.assertTrue(deskTemplate.hasOwnProperty('templateName'));
        }));
  },
]);
