// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.sendMessage('top_frame_only');

document.addEventListener('prerenderingchange', () => {
  chrome.runtime.sendMessage('activated');
});
