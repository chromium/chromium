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
    .then(blob_url_from_background_page => fetch(blob_url_from_background_page))
    .then(http_response_from_fetch => http_response_from_fetch.text())
    .then(blob_contents_as_text => doSendMessage(blob_contents_as_text))
    .catch(error => doSendMessage(error));
