// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener((/** @type{!Object} */ e) => {
  // Send the transformed launch event back to the text extension as soon as
  // this Chrome app is launched by it.
  chrome.runtime.sendMessage('pkplfbidichfdicaijlchgnapepdginl', {
    id: e.id,
    items: (e.items || []).map(item => ({
                                 type: item.type,
                                 entryName: item.entry.name,
                               })),
  });
});
