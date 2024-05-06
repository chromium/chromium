// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testGetPdfThumbnail() {
  chrome.imageLoaderPrivate.getPdfThumbnail(
      'filesystem:chrome-extension://dflhahjnheihhcfhnhcflfdgacjoocip/' +
          'external/Downloads-user/test.pdf',
      100, 100, (thumbnailDataUrl) => {
        chrome.test.assertTrue(
            thumbnailDataUrl.startsWith('data:image/png;base64,'));
        chrome.test.succeed();
      });
}

// For https://crbug.com/334272439
function testGetPdfThumbnailNeedsUtilitySandbox() {
  chrome.imageLoaderPrivate.getPdfThumbnail(
      'filesystem:chrome-extension://dflhahjnheihhcfhnhcflfdgacjoocip/' +
          'external/Downloads-user/combobox_form.pdf',
      100, 100, (thumbnailDataUrl) => {
        chrome.test.assertTrue(
            thumbnailDataUrl.startsWith('data:image/png;base64,'));
        chrome.test.succeed();
      });
}

function testNonExistentFile() {
  chrome.imageLoaderPrivate.getPdfThumbnail(
      'filesystem:chrome-extension://dflhahjnheihhcfhnhcflfdgacjoocip/' +
          'external/Downloads-user/no_such.pdf',
      100, 100, () => {
        chrome.test.assertLastError('Failed to read PDF file');
        chrome.test.succeed();
      });
}

function testWrongUrlScheme() {
  chrome.imageLoaderPrivate.getPdfThumbnail(
      'https://localhost/test.pdf', 100, 100, () => {
        chrome.test.assertLastError('Expected a native local URL');
        chrome.test.succeed();
      });
}

chrome.test.runTests([
  testGetPdfThumbnail, testGetPdfThumbnailNeedsUtilitySandbox,
  testNonExistentFile, testWrongUrlScheme
]);
