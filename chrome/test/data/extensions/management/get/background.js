// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.management.get('pkplfbidichfdicaijlchgnapepdginl', function(info) {
  chrome.test.assertNe(null, info);
  chrome.test.assertEq('pkplfbidichfdicaijlchgnapepdginl', info.id);
  chrome.test.assertEq("simple_extension", info.name);
  chrome.test.assertEq("extension", info.type);
  chrome.test.assertTrue(info.enabled);
  chrome.test.sendMessage("success");
});
