// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Ensure we started with an empty profile.
chrome.test.assertEq(document.cookie, "");

// Set some cookies with a few different modifiers.
var expire = new Date();
expire.setDate(expire.getDate() + 1);  // tomorrow
document.cookie = "a=1";
document.cookie = "b=2; path=/; domain=" + location.host;
document.cookie = "c=3; path=/; expires=" + expire +
                  "; domain=" + location.host;

// Open a tab. This doesn't really prove we're writing to disk, but it is
// difficult to prove that without shutting down the process.
chrome.tabs.create({url: "tab.html"});
