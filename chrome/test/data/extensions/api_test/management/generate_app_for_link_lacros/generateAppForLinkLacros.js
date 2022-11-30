// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runWithUserGesture(() => {
  const url = 'http://google.com';
  const title = 'testApp';
  chrome.management.generateAppForLink(
      url, title, () => {
      chrome.test.assertLastError('Failed to install the generated app.');
      chrome.test.succeed();
  });
});
