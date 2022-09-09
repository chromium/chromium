// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(config => {
  const url = config.customArg;
  if (!url) {
    chrome.test.fail('No customArg URL provided.');
    return;
  }
  chrome.fileManagerPrivate.openURL(url);
  chrome.test.succeed();
});
