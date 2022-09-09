// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// We're just testing that multiple scripts get added to a page in-order
// and are run.
chrome.test.assertEq("hi!", test);

// Also test the injection point is consistent. We want to inject into body
// because having a document.body element is convenient for some scripts.
var scripts = document.querySelectorAll("script");
chrome.test.assertEq(2, scripts.length);
for (var i = 0, script; script = scripts[i]; i++) {
  chrome.test.assertEq("BODY", script.parentElement.nodeName);
}

chrome.test.notifyPass();
