// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var TEST_CASES = [
  // Tests loading a standard 128px icon.
  {
    url: 'chrome://extension-icon/gbmgkahjioeacddebbnengilkgbkhodg/128/0',
    expectedSize: 128
  },
  // Tests loading a standard 48px icon with ExtensionIconSet::Match::kExactly.
  // This should not be resized to 48px.
  {
    url: 'chrome://extension-icon/gbmgkahjioeacddebbnengilkgbkhodg/48/2',
    expectedSize: 32
  },
  // Tests loading a standard 32px icon, grayscale. We assume that we actually
  // got a grayscale image back here.
  {
    url: 'chrome://extension-icon/gbmgkahjioeacddebbnengilkgbkhodg/' +
        '32/1?grayscale=true',
    expectedSize: 32
  },
  // Tests loading a 16px by resizing the 32px version
  // (ExtensionIconSet::Match::kExactly). This should be resized to 16px.
  {
    url: 'chrome://extension-icon/gbmgkahjioeacddebbnengilkgbkhodg/16/1',
    expectedSize: 16
  }
];

var loadedImageCount = 0;

TEST_CASES.forEach(function(testCase) {
  var img = document.createElement('img');
  img.onload = function() {
    if (img.naturalWidth != testCase.expectedSize ||
        img.naturalHeight != testCase.expectedSize) {
      document.title = 'Incorrect size on ' + testCase.url +
          ' Expected: ' + testCase.expectedSize + 'x' + testCase.expectedSize +
          ' Actual: ' + img.naturalWidth + 'x' + img.naturalHeight;
      return;
    }

    if (++loadedImageCount == TEST_CASES.length) {
      document.title = 'Loaded';
    }
  };
  img.onerror = function() {
    // We failed to load an image that should have loaded.
    document.title = 'Couldn\'t load ' + testCase.url;
  };
  img.src = testCase.url;
  document.body.appendChild(img);
});
