// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const SAME_ORIGIN_CHROME_EXTENSION_URL =
    chrome.runtime.getURL('handler.html?protocol=%s');

function crossOriginLocalhostURLFromPort(port) {
  return `http://localhost:${port}/extensions/api_test/protocol_handler/`
}

const TITLE = 'register protocol handler title';
