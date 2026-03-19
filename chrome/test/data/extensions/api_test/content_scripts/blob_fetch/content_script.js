// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function doSendMessage(message) {
  return new Promise(resolve => {
    chrome.runtime.sendMessage(chrome.runtime.id, message, {}, resolve);
  });
}

// This is the content script part of testBlobUrlFromContentScript(). We ask the
// background page to generate a blob:chrome-extension:// URL, then fetch() it
// here in the content script, and pass the response back to the background
// page, which passes the test if we were able to successfully fetch the blob.
doSendMessage('kindly_reply_with_blob_url')
    .then(blobUrlFromBackgroundPage => fetch(blobUrlFromBackgroundPage))
    .then(httpResponseFromFetch => httpResponseFromFetch.text())
    .then(blobContentsAsText => doSendMessage(blobContentsAsText))
    .catch(error => doSendMessage(error));
