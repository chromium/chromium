// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function checkResultFactory(expected) {
  return function() {
    if (expected) {
      chrome.test.succeed();
    } else {
      chrome.test.fail();
    }
  };
}

function TestImageLoadFactory(url, shouldLoad) {
  return function() {
    testImage = document.createElement('img');
    document.body.appendChild(testImage);
    testImage.addEventListener('load', checkResultFactory(shouldLoad));
    testImage.addEventListener('error', checkResultFactory(!shouldLoad));

    testImage.src = url;
  };
}

chrome.test.getConfig(function(config) {
  const customArg = JSON.parse(config.customArg);
  const galleryId = customArg[0];
  const profilePath = customArg[1];
  const extensionUrl = chrome.runtime.getURL('/');
  const extensionId = extensionUrl.split('/')[2];

  const badMountPoint = `filesystem:${extensionUrl}` +
      `external/invalid-${profilePath}-${extensionId}-${galleryId}` +
      '/test.jpg';

  const badMountName = `filesystem:${extensionUrl}` +
      `external/media_galleries-${profilePath}-${galleryId}/test.jpg`;

  const goodUrl = `filesystem:${extensionUrl}` +
      `external/media_galleries-${profilePath}-${extensionId}-` +
      `${galleryId}/test.jpg`;

  chrome.test.runTests([
    TestImageLoadFactory(badMountPoint, false),
    TestImageLoadFactory(badMountName, false),
    TestImageLoadFactory(goodUrl, true),
  ]);
});
