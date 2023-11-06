// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function registerScript() {
  const script =
      {
        id: 'script',
        matches: ['http://example.com/*'],
        runAt: 'document_end',
        js: ['script.js']
      };
  await chrome.scripting.registerContentScripts([script]);
  chrome.test.succeed();
}
