// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.webRequest.onAuthRequired.addListener((_details, callback) => {
  callback({cancel: true});
}, { urls: ["<all_urls>"] }, ["asyncBlocking"]);
