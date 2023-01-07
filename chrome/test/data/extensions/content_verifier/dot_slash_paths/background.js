// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.tabs.create({url:"./page.html"});
chrome.tabs.create({url:"dir/page2.html"});

if (chrome.test)
  chrome.test.sendMessage("background ok");
